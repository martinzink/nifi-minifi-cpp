use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};

pub(crate) struct PerChannelF32 {

}

impl PropertySchema for PerChannelF32 {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for PerChannelF32 {
    type Output = Vec<f32>;

    fn parse(input: &str) -> Result<Self::Output, MinifiError> {
        let parts: Vec<&str> = input.split(',').map(|s| s.trim()).collect();
        match parts.len() {
            1 | 3 => parts
                .iter()
                .map(|p| {
                    p.parse::<f32>().map_err(|e| MinifiError::from(e))
                })
                .collect(),
            _n => Err(MinifiError::parse_err()),  // TODO(mzink) better err
        }
    }
}


#[cfg(test)]
mod tests {
    use minifi_native::PropertyType;
    use crate::utils::per_channel_f32::PerChannelF32;

    #[test]
    fn test_parse_per_channel_f32_accepts_scalar_and_triple() {
        assert_eq!(PerChannelF32::parse("0.5").unwrap(), vec![0.5]);
        assert_eq!(
            PerChannelF32::parse("0.485, 0.456, 0.406").unwrap(),
            vec![0.485, 0.456, 0.406]
        );
        assert!(PerChannelF32::parse("1, 2").is_err());
        assert!(PerChannelF32::parse("1, 2, three").is_err());
    }
}
