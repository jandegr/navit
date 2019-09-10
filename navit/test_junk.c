
void navit_foo_bar(struct point_rect * r, struct point * in, int count_in, struct point *out,
                                  int* count_out) {
    
    const int limit=10000;
    
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

    /* clip all four directions of a rectangle */
    for (edge = 0 ; edge < 4 ; edge++) {
        int i;
        
        struct point *p=pin;
       
        struct point *s=pin+count-1;
        
        *count_out=0;

        /* iterate all points in current buffer */
        for (i = 0 ; i < count ; i++) {
            if (is_inside(p, r, edge)) {
                if (! is_inside(s, r, edge)) {
                    struct point pi;
                    
                    poly_intersection(s,p,r,edge,&pi);
                    pout[(*count_out)++]=pi;
                }
               
                pout[(*count_out)++]=*p;
            } else {
                if (is_inside(s, r, edge)) {
                    struct point pi;
                    
                    poly_intersection(p,s,r,edge,&pi);
                    pout[(*count_out)++]=pi;
                }
                
            }
            
            s=p;
            p++;
        }
        
        count=*count_out;

        if(pout == temp) {
            pout=out;
            pin=temp;
        } else {
            pin=out;
            pout=temp;
        }
    }

    
    if (count_in >= limit) {
        g_free(temp);
    }
    return;
}
