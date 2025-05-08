unsafe extern "C" {
    fn create_integer(value: i32) -> usize;
    fn to_constructor(expression: usize) -> usize;
    fn debug_print(expression: usize);
    fn initialize_jit();
    fn create_empty_list_of_types() -> usize;
    fn create_integer_type() -> usize;
    fn create_pointer_type() -> usize;
    fn compile_expression(expression: usize, return_type: usize, parameters_type: usize) -> usize;
}

fn main() {
    unsafe { initialize_jit() };
    let expression = unsafe { to_constructor(create_integer(10)) };
    unsafe { debug_print(expression) };
    let ptr = unsafe { compile_expression(expression, create_pointer_type(), create_empty_list_of_types()) };
    let ptr: unsafe fn() -> usize = unsafe { std::mem::transmute(ptr) };
    let expression = unsafe { ptr() };
    unsafe { debug_print(expression) };
    let ptr = unsafe { compile_expression(expression, create_integer_type(), create_empty_list_of_types()) };
    let ptr: unsafe fn() -> i32 = unsafe { std::mem::transmute(ptr) };
    let result = unsafe { ptr() };
    println!("{}", result);
}
