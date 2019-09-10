/*cyclomatic complexity = 15*/
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


/*cyclomatic complexity = 14*/
void navit_foo_bar_after(struct point_rect * r, struct point * in, int count_in, struct point *out,
                                  int* count_out) {
    
    const int high_number=5689;
    struct point *temp;
    struct point *pout;
    struct point *pin;
    int edge;
    int count;

    
    if((r == NULL) || (in == NULL) || (out == NULL) || (count_out == NULL) || (*count_out < count_in*8+1)) {
        return;
    }

    // this has an effect of -1
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

