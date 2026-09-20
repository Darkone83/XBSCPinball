#include "xb_xtl_compat.h"
#include "xb_exit.h"

void XbReturnToDashboard(void)
{
    XLaunchNewImage(NULL, NULL);
    for (;;) Sleep(1000);
}
