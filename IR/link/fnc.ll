@tmp = external global i32

define external i32 @fnc() {
	%ret = call i32 @inner()
	ret i32 %ret
}

define private i32 @inner() {
	store i32 120, i32* @tmp
	ret i32 57
}
