@str = constant [4 x i8] c"%f\0A\00"

declare external i32 @printf(i8*, ...)
declare external double @sin(double)
declare external double @cos(double)

@pointer = global double (double)* null

define external void @select(i1 %b) {
	%p = select i1 %b, double (double)* @sin, double (double)* @cos
	store double (double)* %p, double (double)** @pointer
	ret void
}

define external i32 @main() {
	call void @select(i1 true)
	%p = load double (double)*, double (double)** @pointer
	%x = call double %p(double 1.0)
	call i32 (i8*, ...) @printf(i8* getelementptr ([4 x i8], [4 x i8]* @str, i64 0, i64 0), double %x)
	ret i32 0
}
