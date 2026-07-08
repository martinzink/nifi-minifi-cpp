#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) struct Dimensions {
    pub(crate) width: f32,
    pub(crate) height: f32,
}

impl Dimensions {
    pub(crate) fn from_image(img: &image::DynamicImage) -> Self {
        Self {
            width: img.width() as f32,
            height: img.height() as f32,
        }
    }
}
