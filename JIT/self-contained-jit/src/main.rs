use std::ffi::{c_char, c_int, c_void};

#[allow(unused)]
unsafe extern "C" {
    fn get_boolean_type() -> *const c_void;
    fn get_integer_type() -> *const c_void;
    fn get_size_type() -> *const c_void;
    fn get_string_type() -> *const c_void;
    fn debug_print(expression: *const c_void);
    fn to_constructor(expression: *const c_void) -> *const c_void;
    fn create_parameter(index: i32) -> *const c_void;
    fn create_boolean(value: bool) -> *const c_void;
    fn create_integer(value: i32) -> *const c_void;
    fn create_add_integer(left: *const c_void, right: *const c_void) -> *const c_void;
    fn create_size(value: usize) -> *const c_void;
    fn create_string(length: usize, pointer: *const u8) -> *const c_void;
    fn create_print(expression: *const c_void) -> *const c_void;
    fn create_array(
        element_type: *const c_void,
        num_elements: usize,
        elements: *const *const c_void,
    ) -> *const c_void;
    fn create_function(
        name: *const c_char,
        return_type: *const c_void,
        num_parameters: usize,
        parameters_type: *const *const c_void,
        is_variadic: bool,
    ) -> *const c_void;
    fn create_call(
        function: *const c_void,
        return_type: *const c_void,
        num_parameters: usize,
        parameters_ty: *const *const c_void,
        is_variadic: bool,
        arguments: *const *const c_void,
    ) -> *const c_void;
    fn initialize_jit();
    fn create_context() -> *const c_void;
    fn add_function(
        context: *const c_void,
        function_name: *const c_char,
        return_type: *const c_void,
        num_parameters: usize,
        parameters_type: *const *const c_void,
        num_blocks: usize,
    ) -> *const c_void;
    fn set_insert_point(context: *const c_void, block_index: usize);
    fn add_expression(context: *const c_void, expression: *const c_void);
    fn add_return(context: *const c_void, expression: *const c_void);
    fn compile(
        context: *const c_void,
        function_name: *const c_char,
    ) -> unsafe extern "C" fn() -> i32;
    fn delete_context(context: *const c_void);
}

#[unsafe(no_mangle)]
extern "C" fn add_100(x: c_int) -> c_int {
    let ret = x + 100;
    println!("{x} + 100 = {ret}");
    ret
}

fn main() {
    unsafe { initialize_jit() };
    let result = unsafe {
        let context = create_context();
        add_function(
            context,
            c"0".as_ptr(),
            get_integer_type(),
            1,
            &[get_integer_type()] as *const _,
            1,
        );
        set_insert_point(context, 0);
        add_return(
            context,
            create_add_integer(
                create_parameter(0),
                create_call(
                    to_constructor(create_parameter(0)),
                    get_integer_type(),
                    1,
                    &[get_integer_type()] as *const _,
                    false,
                    &[create_integer(1)] as *const _,
                ),
            ),
        );
        add_function(
            context,
            c"main".as_ptr(),
            get_integer_type(),
            0,
            0 as *const _,
            1,
        );
        set_insert_point(context, 0);
        add_expression(
            context,
            create_call(
                create_function(
                    c"add_100".as_ptr(),
                    get_integer_type(),
                    1,
                    &[get_integer_type()] as *const _,
                    false,
                ),
                get_integer_type(),
                1,
                &[get_integer_type()] as *const _,
                false,
                &[create_call(
                    create_function(
                        c"0".as_ptr(),
                        get_integer_type(),
                        1,
                        &[get_integer_type()] as *const _,
                        false,
                    ),
                    get_integer_type(),
                    1,
                    &[get_integer_type()] as *const _,
                    false,
                    &[create_integer(10)] as *const _,
                )] as *const _,
            ),
        );
        add_return(context, create_integer(42));
        let ptr = compile(context, c"main".as_ptr());
        delete_context(context);
        ptr()
    };
    println!("{}", result);
}
