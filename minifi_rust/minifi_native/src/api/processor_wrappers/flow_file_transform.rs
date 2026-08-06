// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use crate::api::InputStream;
use crate::api::flow_file::GetId;
use crate::api::processor::Processor;
use crate::api::processor_wrappers::utils::context_session_flowfile_bundle::ContextSessionFlowFileBundle;
use crate::api::processor_wrappers::utils::flow_file_content::Content;
use crate::api::property::{GetControllerService, GetProperty};
use crate::api::raw_processor::{MultiThreadedTrigger, SingleThreadedTrigger};
use crate::{
    GetAttribute, LogLevel, Logger, MinifiError, MultiThreaded, OnTriggerResult, ProcessContext,
    ProcessError, ProcessSession, Relationship, Schedule, SingleThreaded, info,
};
use std::borrow::Cow;
use std::collections::HashMap;

#[derive(Debug)]
pub struct TransformedFlowFile<'a> {
    target_relationship_name: Cow<'static, str>,
    new_content: Option<Content<'a>>, // If None, the content doesn't change
    attributes_to_add: HashMap<String, String>,
}

impl<'a> TransformedFlowFile<'a> {
    pub fn route_without_changes(target_relationship: &Relationship) -> Self {
        Self::route_without_changes_by_name(Cow::Borrowed(target_relationship.name))
    }

    /// Routes to a relationship identified by name, without changing the content.
    /// Used when routing on error via [`crate::RouteErrorExt`].
    pub fn route_without_changes_by_name(relationship: Cow<'static, str>) -> Self {
        Self {
            target_relationship_name: relationship,
            new_content: None,
            attributes_to_add: HashMap::new(),
        }
    }

    pub fn new(
        target_relationship: &Relationship,
        new_content: Option<Vec<u8>>,
        attributes_to_add: HashMap<String, String>,
    ) -> Self {
        Self {
            target_relationship_name: Cow::Borrowed(target_relationship.name),
            new_content: new_content.map(Content::Buffer),
            attributes_to_add,
        }
    }

    pub fn new_content(&'_ self) -> Option<&'_ Content<'_>> {
        self.new_content.as_ref()
    }

    pub fn target_relationship(&self) -> &str {
        &self.target_relationship_name
    }

    pub fn attributes_to_add(&self) -> &HashMap<String, String> {
        &self.attributes_to_add
    }

    #[cfg(any(test, feature = "test-utils"))]
    pub fn into_bytes(self) -> std::io::Result<Option<Vec<u8>>> {
        match self.new_content {
            Some(Content::Buffer(vec)) => Ok(Some(vec)),
            Some(Content::Stream(mut stream)) => {
                let mut buffer = Vec::new();
                stream.read_to_end(&mut buffer)?;
                Ok(Some(buffer))
            }
            None => Ok(None),
        }
    }
}

pub trait FlowFileTransform {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError>;
}

pub trait MutFlowFileTransform {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute,
        LoggerImpl: Logger,
    >(
        &mut self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError>;
}

pub struct FlowFileTransformProcessorType {}

fn handle_transform<PC, PS, L, F>(
    context: &mut PC,
    session: &mut PS,
    logger: &L,
    mut transform_fn: F,
) -> Result<OnTriggerResult, ProcessError>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
    L: Logger,
    F: for<'stream> FnMut(
        &ContextSessionFlowFileBundle<'_, PC, PS>,
        &'stream mut dyn InputStream,
    ) -> Result<TransformedFlowFile<'stream>, ProcessError>,
{
    if let Some(mut flow_file) = session.get() {
        let simple_context = ContextSessionFlowFileBundle::new(context, session, Some(&flow_file));

        let (attrs_to_add, relationship) = session.read_stream(&flow_file, |input_stream| {
            let transformed = match transform_fn(&simple_context, input_stream) {
                Ok(transform_success) => transform_success,
                Err(ProcessError::Route(route)) => {
                    route.log(logger);
                    TransformedFlowFile::route_without_changes_by_name(route.relationship)
                }
                // A real error: propagate as MinifiError so the trigger fails
                // and the agent rolls back the session.
                Err(ProcessError::Fatal(e)) => {
                    return Err(e);
                }
            };

            info!(logger, "{:?}", transformed);
            match transformed.new_content {
                None => {}
                Some(Content::Buffer(buffer)) => {
                    session.write(&flow_file, &buffer)?;
                }
                Some(Content::Stream(stream)) => {
                    session.write_from_stream(&flow_file, stream)?;
                }
            };

            Ok((
                transformed.attributes_to_add,
                transformed.target_relationship_name,
            ))
        })?;

        for (k, v) in attrs_to_add {
            session.set_attribute(&mut flow_file, &k, &v)?;
        }

        session.transfer(flow_file, relationship.as_ref())?;
        Ok(OnTriggerResult::Ok)
    } else {
        logger.log(LogLevel::Trace, format_args!("No flowfile to transform"));
        Ok(OnTriggerResult::Yield)
    }
}

impl<Implementation, L> MultiThreadedTrigger
    for Processor<Implementation, FlowFileTransformProcessorType, MultiThreaded, L>
where
    Implementation: Schedule + FlowFileTransform,
    L: Logger,
{
    fn trigger<PC, PS>(
        &self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref scheduled_impl) = self.scheduled_impl {
            handle_transform(context, session, &self.logger, |ctx, input| {
                scheduled_impl.transform(ctx, input, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err("The processor hasn't been scheduled yet").into())
        }
    }
}

impl<Implementation, L> SingleThreadedTrigger
    for Processor<Implementation, FlowFileTransformProcessorType, SingleThreaded, L>
where
    Implementation: Schedule + MutFlowFileTransform,
    L: Logger,
{
    fn trigger<PC, PS>(
        &mut self,
        context: &mut PC,
        session: &mut PS,
    ) -> Result<OnTriggerResult, ProcessError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
    {
        if let Some(ref mut scheduled_impl) = self.scheduled_impl {
            handle_transform(context, session, &self.logger, |ctx, input| {
                scheduled_impl.transform(ctx, input, &self.logger)
            })
        } else {
            Err(MinifiError::trigger_err("The processor hasn't been scheduled yet").into())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::RawProcessor;
    use crate::api::raw_processor::MultiThreadedTrigger;
    use crate::{
        GetControllerService, GetId, MockFlowFile, MockLogger, MockProcessContext,
        MockProcessSession, ProcessError, RouteErrorExt,
    };

    /// A transform whose fallible work fails and is routed via `route_to_failure`.
    struct RouteToFailure;
    impl Schedule for RouteToFailure {
        fn schedule<Ctx: GetProperty, L: Logger>(_c: &Ctx, _l: &L) -> Result<Self, MinifiError> {
            Ok(RouteToFailure)
        }
    }
    impl FlowFileTransform for RouteToFailure {
        fn transform<
            'a,
            Context: GetProperty + GetControllerService + GetAttribute + GetId,
            LoggerImpl: Logger,
        >(
            &self,
            _context: &Context,
            _input_stream: &'a mut dyn InputStream,
            _logger: &LoggerImpl,
        ) -> Result<TransformedFlowFile<'a>, ProcessError> {
            let bad: Result<TransformedFlowFile<'a>, std::io::Error> = Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "bad data",
            ));
            bad.route_err_to_failure()
        }
    }

    /// A transform that hits a real (fatal) error.
    struct FatalTransform;
    impl Schedule for FatalTransform {
        fn schedule<Ctx: GetProperty, L: Logger>(_c: &Ctx, _l: &L) -> Result<Self, MinifiError> {
            Ok(FatalTransform)
        }
    }
    impl FlowFileTransform for FatalTransform {
        fn transform<
            'a,
            Context: GetProperty + GetControllerService + GetAttribute + GetId,
            LoggerImpl: Logger,
        >(
            &self,
            _context: &Context,
            _input_stream: &'a mut dyn InputStream,
            _logger: &LoggerImpl,
        ) -> Result<TransformedFlowFile<'a>, ProcessError> {
            Err(ProcessError::Fatal(MinifiError::trigger_err("real error")))
        }
    }

    fn seeded_session() -> MockProcessSession {
        let mut session = MockProcessSession::new();
        session
            .input_flow_files
            .push(MockFlowFile::with_content(b"data"));
        session
    }

    #[test]
    fn route_error_transfers_to_failure_and_commits() {
        let mut processor: Processor<
            RouteToFailure,
            FlowFileTransformProcessorType,
            MultiThreaded,
            MockLogger,
        > = Processor::new(MockLogger::new());
        processor.scheduled_impl = Some(RouteToFailure);

        let mut context = MockProcessContext::new();
        let mut session = seeded_session();

        let result = MultiThreadedTrigger::trigger(&processor, &mut context, &mut session);

        // Routing on error is a committed success, not a rollback.
        assert_eq!(
            result.expect("should commit, not roll back"),
            OnTriggerResult::Ok
        );
        let transferred = session.transferred_flow_files.borrow();
        assert_eq!(transferred.len(), 1);
        assert_eq!(transferred[0].relationship, "failure");
    }

    #[test]
    fn fatal_error_propagates_and_transfers_nothing() {
        let mut processor: Processor<
            FatalTransform,
            FlowFileTransformProcessorType,
            MultiThreaded,
            MockLogger,
        > = Processor::new(MockLogger::new());
        processor.scheduled_impl = Some(FatalTransform);

        let mut context = MockProcessContext::new();
        let mut session = seeded_session();

        let result = MultiThreadedTrigger::trigger(&processor, &mut context, &mut session);

        assert!(matches!(
            result,
            Err(ProcessError::Fatal(MinifiError::TriggerError(_)))
        ));
        // A fatal error must not transfer the flow file anywhere (session rolls back).
        assert_eq!(session.num_of_transferred_flow_files(), 0);
    }
}
