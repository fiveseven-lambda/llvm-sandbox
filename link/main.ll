@str = constant [4 x i8] c"%d\0A\00"

declare i32 @printf(i8*, ...)

declare i32 @fnc()

define i32 @main() {
	%x = call i32 @fnc()
	call i32 (i8*, ...) @printf(i8* getelementptr ([4 x i8], [4 x i8]* @str, i64 0, i64 0), i32 %x)
	ret i32 0
}
