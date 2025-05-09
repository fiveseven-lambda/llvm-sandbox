unsafe extern "C" {
    fn create_integer(value: i32) -> usize;
    fn to_constructor(expression: usize) -> usize;
    fn debug_print(expression: usize);
    fn initialize_jit();
    fn create_integer_type() -> usize;
    fn create_pointer_type() -> usize;
    fn compile_expression(
        expression: usize,
        return_type: usize,
        parameters_type: usize,
        num_parameters: usize,
    ) -> usize;
}

fn main() {
    unsafe { initialize_jit() };
    let mut expression = unsafe { create_integer(10) };
    for _ in 0..5 {
        expression = unsafe { to_constructor(expression) };
    }
    for _ in 0..5 {
        unsafe { debug_print(expression) };
        let ptr = unsafe { compile_expression(expression, create_pointer_type(), 0, 0) };
        let ptr: unsafe fn() -> usize = unsafe { std::mem::transmute(ptr) };
        expression = unsafe { ptr() };
    }
    unsafe { debug_print(expression) };
    let ptr = unsafe { compile_expression(expression, create_integer_type(), 0, 0) };
    let ptr: unsafe fn() -> i32 = unsafe { std::mem::transmute(ptr) };
    let result = unsafe { ptr() };
    println!("{}", result);
}
