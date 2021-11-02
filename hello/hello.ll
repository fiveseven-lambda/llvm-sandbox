@str = constant [6 x i8] c"Hello\00"

declare i32 @puts(i8*)

define i32 @main() {
	call i32 @puts(i8* getelementptr ([6 x i8], [6 x i8]* @str, i64 0, i64 0))
	ret i32 0
}
