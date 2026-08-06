use minifi_native::{GetAttribute, MinifiError};
use tract::__ndarray_interop::TensorInterface;
use tract::Tensor;

pub(crate) struct TensorFlowFile<'a> {
    raw: &'a [u8],
}

fn bytes_to_f32s(bytes: &[u8]) -> Vec<f32> {
    bytes
        .chunks_exact(4)
        .map(|c| f32::from_le_bytes(c.try_into().unwrap()))
        .collect()
}

impl<'a> TensorFlowFile<'a> {
    pub(crate) fn new(raw: &'a [u8]) -> Self {
        Self { raw }
    }
    pub(crate) fn get_f32_slice<Ctx: GetAttribute>(
        &self,
        context: &Ctx,
        target_index: usize,
    ) -> Result<Vec<f32>, MinifiError> {
        let slice = self.get_slice(context, target_index)?;
        Ok(bytes_to_f32s(slice))
    }

    fn get_slice<Ctx: GetAttribute>(
        &self,
        context: &Ctx,
        target_index: usize,
    ) -> Result<&'a [u8], MinifiError> {
        let mut cursor = 0usize;
        for i in 0..=target_index {
            let len = Self::read_output_bytes(context, i)?;
            if i == target_index {
                let end = cursor + len;
                if end > self.raw.len() {
                    return Err(MinifiError::trigger_err(format!(
                        "Payload smaller than declared output {} slice",
                        target_index
                    )));
                }
                return Ok(&self.raw[cursor..end]);
            }
            cursor += len;
        }
        unreachable!()
    }

    fn read_output_bytes<Ctx: GetAttribute>(context: &Ctx, i: usize) -> Result<usize, MinifiError> {
        let attr_name = format!("tract.output.{}.bytes", i);
        let raw = context
            .get_attribute(&attr_name)?
            .ok_or_else(|| MinifiError::trigger_err(format!("Missing {} attribute", attr_name)))?;
        raw.parse::<usize>()
            .map_err(|e| MinifiError::trigger_err(format!("Invalid {}: {}", attr_name, e)))
    }
}

pub(crate) fn tensor_as_f32(tensors: &[Tensor], index: usize) -> Result<Vec<f32>, MinifiError> {
    let tensor = tensors
        .get(index)
        .ok_or(MinifiError::trigger_err("Invalid shape of tensor"))?;
    let (_datum_type, _shape, raw_bytes) = tensor.as_bytes().map_err(MinifiError::custom)?;
    Ok(bytes_to_f32s(raw_bytes))
}
