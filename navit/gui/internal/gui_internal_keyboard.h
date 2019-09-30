/* prototypes */
struct gui_priv;
struct widget;
struct widget *gui_internal_keyboard_do(struct gui_priv *this, struct widget *wkbdb, int mode);
struct widget *gui_internal_keyboard(struct gui_priv *this, int mode);
struct widget *gui_internal_keyboard_show_native(struct gui_priv *this, struct widget *w, int mode, char *lang);
int gui_internal_keyboard_init_mode(char *lang);
/* end of prototypes */
