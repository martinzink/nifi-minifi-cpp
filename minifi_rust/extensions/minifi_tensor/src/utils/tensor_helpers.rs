use minifi_native::{GetAttribute, InputStream, MinifiError};
use strum_macros::{Display, EnumString};
use tract::__ndarray_interop::TensorInterface;
use tract::Tensor;
use tract::prelude::DatumType;
tract::impl_ndarray_interop!();

fn bytes_to_f32s(bytes: &[u8]) -> Vec<f32> {
    bytes
        .chunks_exact(4)
        .map(|c| f32::from_le_bytes(c.try_into().unwrap()))
        .collect()
}

fn parse_req_usize_attr<Context: GetAttribute>(
    context: &Context,
    key: &str,
) -> Result<usize, MinifiError> {
    context
        .get_required_attribute(key)
        .and_then(|s| s.parse::<usize>().map_err(|e| e.into()))
}

fn parse_tensors_len<Context: GetAttribute>(context: &Context) -> Result<usize, MinifiError> {
    parse_req_usize_attr(context, "tensors.len")
}

fn parse_tensor_len<Context: GetAttribute>(
    context: &Context,
    id: usize,
) -> Result<usize, MinifiError> {
    parse_req_usize_attr(context, &format!("tensor.{}.bytes", id))
}

fn parse_tensor_shape<Context: GetAttribute>(
    context: &Context,
    id: usize,
) -> Result<Vec<usize>, MinifiError> {
    let shape_str = context.get_required_attribute(&format!("tensor.{}.shape", id))?;

    if shape_str.trim().is_empty() {
        return Ok(Vec::new());
    }

    let shape = shape_str
        .split(',')
        .map(|s| s.trim().parse::<usize>())
        .collect::<Result<Vec<usize>, _>>()?;

    Ok(shape)
}

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString)]
#[strum(serialize_all = "PascalCase", const_into_str)]
enum MinifiDatumType {
    F32,
}

impl Into<DatumType> for MinifiDatumType {
    fn into(self) -> DatumType {
        match self {
            MinifiDatumType::F32 => DatumType::F32,
        }
    }
}

fn parse_tensor_dtype<Context: GetAttribute>(
    context: &Context,
    id: usize,
) -> Result<DatumType, MinifiError> {
    let shape_str = context.get_required_attribute(&format!("tensor.{}.dtype", id))?;
    let dtype = shape_str.parse::<MinifiDatumType>()?;
    Ok(dtype.into())
}

pub(crate) fn deserialize_tensors<Context: GetAttribute>(
    context: &Context,
    input_stream: &mut dyn InputStream,
) -> Result<Vec<Tensor>, MinifiError> {
    let mut result = vec![];

    let mut flow_file_contents = Vec::new();
    input_stream.read_to_end(&mut flow_file_contents)?;
    let number_of_tensors = parse_tensors_len(context)?;

    let mut cursor = 0usize;
    for i in 0..=number_of_tensors {
        let tensor_len = parse_tensor_len(context, i)?;
        let tensor_shape = parse_tensor_shape(context, i)?;
        let tensor_dtype = parse_tensor_dtype(context, i)?;
        let tensor_data = &flow_file_contents[cursor..cursor + tensor_len];
        result.push(
            Tensor::from_bytes(tensor_dtype, &tensor_shape, tensor_data)
                .map_err(MinifiError::custom)?,
        );
        cursor += tensor_len;
    }

    Ok(result)
}

pub(crate) fn tensor_as_f32(tensors: &[Tensor], index: usize) -> Result<Vec<f32>, MinifiError> {
    let tensor = tensors
        .get(index)
        .ok_or(MinifiError::trigger_err("Invalid shape of tensors"))?;
    let (_datum_type, _shape, raw_bytes) = tensor.as_bytes().map_err(MinifiError::custom)?;
    Ok(bytes_to_f32s(raw_bytes))
}
