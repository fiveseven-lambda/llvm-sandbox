unsafe extern "C" {
    fn create_integer(value: i32) -> usize;
    fn to_constructor(expression: usize) -> usize;
    fn debug_print(expression: usize);
    fn initialize_jit();
    fn create_empty_list_of_types() -> usize;
    fn create_pointer_type() -> usize;
    fn compile_expression(expression: usize, return_type: usize, parameters_type: usize) -> usize;
}

fn main() {
    unsafe { initialize_jit() };
    let expression = unsafe { to_constructor(create_integer(10)) };
    unsafe { debug_print(expression) };
    unsafe { compile_expression(expression, create_pointer_type(), create_empty_list_of_types()) };
}
