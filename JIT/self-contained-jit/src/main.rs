#[allow(unused)]
unsafe extern "C" {
    fn get_integer_type() -> usize;
    fn get_size_type() -> usize;
    fn debug_print(expression: usize);
    fn to_constructor(expression: usize) -> usize;
    fn create_integer(value: i32) -> usize;
    fn create_string(length: usize, pointer: *const u8) -> usize;
    fn create_print(expression: usize) -> usize;
    fn initialize_jit();
    fn compile_expression(
        expression: usize,
        return_type: usize,
        num_parameters: usize,
        parameters_type: usize,
    ) -> usize;
}

fn main() {
    unsafe { initialize_jit() };
    let s = "Hello, world!\n";
    let mut expression = unsafe { create_print(create_string(s.len(), s.as_ptr())) };
    for _ in 0..5 {
        expression = unsafe { to_constructor(expression) };
    }
    for _ in 0..5 {
        unsafe { debug_print(expression) };
        let ptr = unsafe { compile_expression(expression, get_size_type(), 0, 0) };
        let ptr: unsafe fn() -> usize = unsafe { std::mem::transmute(ptr) };
        expression = unsafe { ptr() };
    }
    unsafe { debug_print(expression) };
    let ptr = unsafe { compile_expression(expression, get_integer_type(), 0, 0) };
    let ptr: unsafe fn() -> i32 = unsafe { std::mem::transmute(ptr) };
    let result = unsafe { ptr() };
    println!("{}", result);
}
