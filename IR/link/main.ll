@str = constant [8 x i8] c"%d, %d\0A\00"

declare external i32 @printf(i8*, ...)

declare external i32 @fnc()

@tmp = global i32 0

define private i32 @inner() {
	%x = call i32 @fnc()
	%y = load i32, i32* @tmp
	call i32 (i8*, ...) @printf(i8* getelementptr ([8 x i8], [8 x i8]* @str, i64 0, i64 0), i32 %x, i32 %y)
	ret i32 0
}

define external i32 @main() {
	%inner = load i32 (), i32 ()* @inner
	%ret = call i32 %inner()
	ret i32 %ret
}
