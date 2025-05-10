#[allow(unused)]
unsafe extern "C" {
    fn get_integer_type() -> usize;
    fn get_size_type() -> usize;
    fn debug_print(expression: usize);
    fn to_constructor(expression: usize) -> usize;
    fn create_integer(value: i32) -> usize;
    fn create_add_integer(left: usize, right: usize) -> usize;
    fn create_parameter(index: i32) -> usize;
    fn create_string(length: usize, pointer: *const u8) -> usize;
    fn create_print(expression: usize) -> usize;
    fn create_call();
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
    let mut expression = unsafe { create_add_integer(create_parameter(0), create_integer(100)) };
    for _ in 0..3 {
        expression = unsafe { to_constructor(expression) };
    }
    for _ in 0..3 {
        unsafe { debug_print(expression) };
        let ptr = unsafe { compile_expression(expression, get_size_type(), 0, 0) };
        let ptr: unsafe fn() -> usize = unsafe { std::mem::transmute(ptr) };
        expression = unsafe { ptr() };
    }
    unsafe { debug_print(expression) };
    let ptr = unsafe {
        compile_expression(
            expression,
            get_integer_type(),
            1,
            &[get_integer_type()] as *const usize as usize,
        )
    };
    let ptr: unsafe fn(i32) -> i32 = unsafe { std::mem::transmute(ptr) };
    for x in 10..20 {
        let y = unsafe { ptr(x) };
        println!("ptr({x}) = {y}");
    }
}
