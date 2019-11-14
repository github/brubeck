#include "sput.h"
#include "brubeck.h"

static void check_eq(float f, const char *str)
{
	char buf[16];
	brubeck_ftoa(buf, f);
	sput_fail_unless(strcmp(str, buf) == 0, str);
}

void test_ftoa(void)
{
	check_eq(0.0, "0");
	check_eq(15.0, "15");
	check_eq(15.5, "15.5");
	check_eq(15.505, "15.505");
	check_eq(0.125, "0.125");
	check_eq(1234.567, "1234.567");
	check_eq(99999.999, "100000");
	check_eq(0.999, "0.999");
	check_eq(43427902563.12, "43427901440");
}
