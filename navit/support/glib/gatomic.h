#include <glib/gtypes.h>

gint g_atomic_int_add(volatile gint  *atomic,gint val);
gint g_atomic_int_exchange_and_add (volatile gint *atomic,gint val);
