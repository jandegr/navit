/* cyclomatic complexity = 15 */
void navit_foo_bar_before(struct point_rect * r, struct point * in, int count_in, struct point *out,
                                  int* count_out) {
    
    const int limit=5689;
    struct point *temp=g_alloca(sizeof(struct point) * (count_in < limit ? count_in*8+1:0));
    struct point *pout;
    struct point *pin;
    int edge;
    int count;

    
    if((r == NULL) || (in == NULL) || (out == NULL) || (count_out == NULL) || (*count_out < count_in*8+1)) {
        return;
    }

    
    if (count_in >= limit) {      
        temp=g_new(struct point, count_in*8+1);
    }
    
    pout = temp;
    pin=in;
    count=count_in;

    for (edge = 0 ; edge < 4 ; edge++) {
        if (foo) {
            if (is_inside(p, r, edge)) {
                if (! is_inside(s, r, edge)) {
                    some_action();
                }    
             some_action2();  
            } else {
                if (is_inside(s, r, edge)) {
                   some_action3()
                }
            }           
           do_something_here_too();
        }
        
        count=*count_out;

        if(pout == temp) {
            smth();
        } else {
            smth_else();
        }
    }

    if (count_in >= limit) {
        g_free(temp);
    }
    return;
}


/* cyclomatic complexity = 11 */
void navit_foo_bar_after(struct point_rect * r, struct point * in, int count_in, struct point *out,
                                  int* count_out) {
    
    const int high_number=5689;
    struct point *temp;
    struct point *pout;
    struct point *pin;
    int edge;
    int count;

    // this has an effect of -3 on cyclomatic complexity
    // using || does it for codefactor, but using + makes it more univeresal across different scantools
    // however, some inline function for this kind of checks for general usage in Navit can hadle this
    // in a much more elegant way, this is nothing but a demo.
    int has_nullpointer = (r == NULL) + (in == NULL) + (out == NULL) + (count_out == NULL);
    if(has_nullpointer || (*count_out < count_in*8+1)) {
        return;
    }

    // this has an effect of -1 on cyclomatic complexity but for some reason it does not reduce overall
    // complexity in codefactor, I will investigate this further on a rainy day.
    if (count_in >= high_number) {
        temp=g_new(struct point, count_in*8+1);
    } else {
        temp=g_alloca(sizeof(struct point) * (count_in*8+1));
    }
    
    pout = temp;
    pin=in;
    count=count_in;

    for (edge = 0 ; edge < 4 ; edge++) {
        if (foo) {
            if (is_inside(p, r, edge)) {
                if (! is_inside(s, r, edge)) {
                    some_action();
                }    
             some_action2();  
            } else {
                if (is_inside(s, r, edge)) {
                   some_action3()
                }
            }           
           do_something_here_too();
        }
        
        count=*count_out;

        if(pout == temp) {
            smth();
        } else {
            smth_else();
        }
    }

    if (count_in >= high_number) {
        g_free(temp);
    }
    return;
}

