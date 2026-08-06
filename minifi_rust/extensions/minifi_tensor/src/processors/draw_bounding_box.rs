use crate::utils::bounding_box::{BoundingBox, BoundingBoxes};
use image::{ImageFormat, Rgb, load_from_memory};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, OutputAttribute, ProcessError, ProcessorDefinition, ProcessorInputRequirement,
    Property, PropertyConstraints, PropertyType, Relationship, RouteErrorExt, Schedule,
    TransformedFlowFile,
};
use minifi_native::{PropertyDefinition, PropertySchema, property_definitions};
use std::collections::HashMap;
use std::io::Cursor;

pub(crate) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Flowfiles are routed here after drawing the bounding boxes",
};

pub(crate) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Invalid FlowFiles are routed here",
};

pub(crate) const BOUNDING_BOXES: Property<BoundingBoxes> =
    Property::new("Bounding boxes", "TODO(mzink)").with_default("${enrichment.value}");

const LINE_THICKNESS: Property<u32> =
    Property::new("Line tickness", "TODO(mzink)").with_default("5");

const LINE_COLOR: Property<LineColor> =
    Property::new("Line color", "TODO(mzink)").with_default("[0, 255, 0]");

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DrawBoundingBox {}

impl Schedule for DrawBoundingBox {
    fn schedule<Ctx: GetProperty, L: Logger>(
        _context: &Ctx,
        _logger: &L,
    ) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

struct LineColor {}

impl PropertySchema for LineColor {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for LineColor {
    type Output = Rgb<u8>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        let clean_str = s.trim().trim_matches(|c| c == '[' || c == ']');
        let mut iter = clean_str.split(',');

        Ok(Rgb::<u8>([
            iter.next()
                .ok_or(MinifiError::parse_err())?
                .trim()
                .parse::<u8>()?,
            iter.next()
                .ok_or(MinifiError::parse_err())?
                .trim()
                .parse::<u8>()?,
            iter.next()
                .ok_or(MinifiError::parse_err())?
                .trim()
                .parse::<u8>()?,
        ]))
    }
}

impl FlowFileTransform for DrawBoundingBox {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        _logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        let line_thickness = context.get_property(&LINE_THICKNESS)?;
        let line_color = context.get_property(&LINE_COLOR)?;
        let boxes: Vec<BoundingBox> = context
            .get_property(&BOUNDING_BOXES)
            .route_err_to_failure()?;

        let mut image_bytes = Vec::new();
        input_stream.read_to_end(&mut image_bytes)?;

        let mut img = load_from_memory(&image_bytes)
            .map(|dyn_img| dyn_img.to_rgb8())
            .route_err_to_failure()?;

        boxes
            .iter()
            .for_each(|bbox| bbox.draw_onto(&mut img, line_thickness, line_color));

        let mut output_bytes = Vec::new();
        img.write_to(&mut Cursor::new(&mut output_bytes), ImageFormat::Png)
            .route_err_to_failure()?;

        Ok(TransformedFlowFile::new(
            &SUCCESS,
            Some(output_bytes),
            HashMap::new(),
        ))
    }
}

impl ProcessorDefinition for DrawBoundingBox {
    const DESCRIPTION: &'static str = "DrawBoundingBox";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];
    fn properties() -> &'static [PropertyDefinition] {
        const PROPERTIES: &[PropertyDefinition] =
            property_definitions![BOUNDING_BOXES, LINE_COLOR, LINE_THICKNESS];

        PROPERTIES
    }
}

#[cfg(test)]
mod tests {
    use crate::processors::draw_bounding_box::{LINE_COLOR, LINE_THICKNESS};
    use minifi_native::{GetProperty, MockControllerServiceContext};

    #[test]
    fn test_parsing_colors() {
        let mock_context = MockControllerServiceContext::default();
        let default_color = mock_context
            .get_property(&LINE_COLOR)
            .expect("we should parse this");
        let green = image::Rgb([0, 255, 0]);
        assert_eq!(default_color, green);
    }

    #[test]
    fn test_parsing_line_thickness() {
        let mock_context = MockControllerServiceContext::default();
        let default_thickness = mock_context
            .get_property(&LINE_THICKNESS)
            .expect("we should parse this");
        assert_eq!(default_thickness, 5);
    }
}
