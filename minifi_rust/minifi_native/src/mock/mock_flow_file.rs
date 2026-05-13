use crate::api::FlowFile;
use std::cell::RefCell;
use std::collections::HashMap;

pub struct MockFlowFile {
    pub content: RefCell<Vec<u8>>,
    pub attributes: HashMap<String, String>,
}

impl FlowFile for MockFlowFile {}

impl MockFlowFile {
    pub fn new() -> MockFlowFile {
        MockFlowFile {
            content: RefCell::new(Vec::new()),
            attributes: HashMap::new(),
        }
    }

    pub fn with_content(content: &[u8]) -> MockFlowFile {
        Self {
            content: RefCell::new(content.to_vec()),
            attributes: HashMap::new(),
        }
    }

    pub fn content_len(&self) -> usize {
        self.content.borrow().len()
    }

    pub fn content_eq<S>(&self, other: S) -> bool
    where
        S: Into<String>,
    {
        let my_content = self.content.borrow();
        *my_content == other.into().as_bytes()
    }

    pub fn get_stream(&self) -> std::io::Cursor<Vec<u8>> {
        std::io::Cursor::new(self.content.borrow().clone())
    }
}
