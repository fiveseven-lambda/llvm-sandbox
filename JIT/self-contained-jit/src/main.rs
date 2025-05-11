use std::ffi::{c_char, c_int};

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
    fn create_function(
        name: *const c_char,
        return_type: usize,
        num_parameters: usize,
        parameters_type: *const usize,
        is_variadic: bool,
    ) -> usize;
    fn create_call(
        function: usize,
        return_type: usize,
        num_parameters: usize,
        parameters_ty: *const usize,
        is_variadic: bool,
        arguments: *const usize,
    ) -> usize;
    fn initialize_jit();
    fn compile_expression(
        expression: usize,
        return_type: usize,
        num_parameters: usize,
        parameters_type: *const usize,
    ) -> usize;
}

#[unsafe(no_mangle)]
extern "C" fn hello(x: c_int) -> c_int {
    println!("Hello");
    x + 100
}

fn main() {
    unsafe { initialize_jit() };
    let one_integer_type = [unsafe { get_integer_type() }];
    let mut expression = unsafe {
        create_call(
            to_constructor(create_call(
                create_function(
                    c"hello".as_ptr(),
                    get_integer_type(),
                    1,
                    &one_integer_type as *const usize,
                    false,
                ),
                get_integer_type(),
                1,
                &one_integer_type as *const usize,
                false,
                &[create_add_integer(create_parameter(0), create_integer(1))] as *const usize,
            )),
            get_integer_type(),
            1,
            &one_integer_type as *const usize,
            false,
            &[create_integer(10)] as *const usize,
        )
    };
    for _ in 0..5 {
        expression = unsafe { to_constructor(expression) };
    }
    for _ in 0..5 {
        unsafe { debug_print(expression) };
        let ptr = unsafe { compile_expression(expression, get_size_type(), 0, std::ptr::null()) };
        let ptr: unsafe fn() -> usize = unsafe { std::mem::transmute(ptr) };
        expression = unsafe { ptr() };
    }
    unsafe { debug_print(expression) };
    let ptr = unsafe { compile_expression(expression, get_integer_type(), 0, std::ptr::null()) };
    let ptr: unsafe fn() -> i32 = unsafe { std::mem::transmute(ptr) };
    let result = unsafe { ptr() };
    println!("ptr() = {result}");
}
