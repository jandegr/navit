/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

package org.navitproject.navit;

import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Message;
import android.util.FloatMath;
import android.util.Log;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.GestureDetector;
import android.view.inputmethod.InputMethodManager;
import android.widget.RelativeLayout;
import java.util.ArrayList;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ConcurrentLinkedQueue;



/**
 * @brief This function is used to draw the objects to a canvas. 
 * It can be done in multiple threads, but the performance is not good because the sync after each layer takes a very long time. 
 * One thread for drawing the objects is the best atm.
 *
 * @author Sascha Oedekoven (07/2015)
 */

public class NavitDrawObjects extends Thread 
{
	private LinkedBlockingQueue<NavitDrawObject> draw_obj_list = new LinkedBlockingQueue<NavitDrawObject>();
	
	private ArrayList<NavitDrawObject> local_obj_list;
	
	
	//NavitDrawObject obj;
	public boolean run;
	
	private static NavitGraphics ng;
	
	//Canvas which will be shown at the screen
	public Canvas	screen_canvas;
	
	//Bitmap and Canvas which are used to draw on in the thread, they are drawn to the screen_canvas after each Layer has been processed
	private static Canvas[] priv_canvas;
	private static Bitmap[] priv_bitmap;
	
	
	private Paint paint_polyline;
	private Paint paint_polygon;
	
	
	private int w;
	private int h;
	private int thread_num;
	private int idx;
	

	
	private Path path;
	private Paint paint;
	
	
	private long start;
	
	public NavitDrawObjects(Canvas canvas, int w, int h, NavitGraphics navitgraphics, int thread_num) {
		
		start = android.os.SystemClock.elapsedRealtime();

		
		if(ng == null) {
			ng = navitgraphics;
		}
		
		local_obj_list = new ArrayList<NavitDrawObject>();
		paint_polyline = new Paint();
		////paint_polyline.setStrokeJoin(Paint.Join.ROUND);   
		paint_polyline.setStrokeCap(Paint.Cap.ROUND);      
		paint_polyline.setStyle(Paint.Style.STROKE);
		
		paint_polygon = new Paint();
		paint_polygon.setStyle(Paint.Style.FILL);
		
		path = new Path();
		
		
		this.thread_num = thread_num;
		this.w = w;
		this.h = h;
		
		idx = thread_num-1;
		
		//Log.e("NavitDrawObjects", "new drawobjects thread created!");
		screen_canvas = canvas;
		
		if(priv_canvas == null || priv_canvas.length != NavitDrawObjectsPool.thread_n) {
			priv_canvas = new Canvas[NavitDrawObjectsPool.thread_n];
			priv_bitmap = new Bitmap[NavitDrawObjectsPool.thread_n];

		}
		
		if(NavitDrawObjectsPool.thread_n == 1) {
			
			//directly draw to the screen bitmap!	
			priv_bitmap[0] = ng.draw_bitmap;
			priv_canvas[0] = ng.draw_canvas;
			
		} else {
			
			//create private bitmap per thread and draw to this one. combine bitmaps at the end of every layer / at the end of the drawing process
		

			if(priv_canvas[idx] == null) {
				priv_bitmap[idx] = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
				priv_canvas[idx] = new Canvas(priv_bitmap[idx]);
			} else {
				priv_bitmap[idx].eraseColor(0);
			}
			
		}
		
		paint = new Paint();
		
		run = true;
	}

	
	public void run() {
		while(run) {
			
			//try {
				
				
				 //NavitDrawObject tmp = draw_obj_list.take();
				//local_obj_list.add(tmp);
				
				draw_obj_list.drainTo(local_obj_list, 50);
				
				
				//Log.e("NavitDrawObjects", "drained objects: " + local_obj_list.size() + " from thread " + thread_num);
				
				for (NavitDrawObject obj : local_obj_list) {
				
				
				
				
				//if( run && (obj = draw_obj_list.poll()) != null) { // .take -> blocking call, .poll -> non-blocking call

					switch(obj.type) {
						case POLYLINE:
							draw_polyline(obj.paint, obj.c);
							break;
						case POLYGON:
							draw_polygon(obj.paint, obj.c);
							break;
						case RECTANGLE:
							draw_rectangle(obj.paint, obj.x, obj.y, obj.w, obj.h);
							break;
						case TEXT:
							draw_text(obj.x, obj.y, obj.text, obj.size, obj.dx, obj.dy, obj.bgcolor, obj.lw, obj.fgcolor);
							break;
						case CIRCLE:
							draw_circle(obj.paint, obj.x, obj.y, obj.r);
							break;
						case IMAGE:
							draw_image(obj.paint, obj.x, obj.y, obj.bitmap);
							break;
						case TOSCREEN:
							draw_to_screen(obj.mode);
							break;
						default:
							Log.e("NavitDrawObjects", "unknown element!!");
					}

					

				}
				local_obj_list.clear(); 

				//}
				
				
			/*} catch(Exception e) {
				Log.e("NavitDrawObjects", "no object to catch, run == " + run);
			}*/
			
		}
	}
	
	public void cancel_draw() {
		
		//Log.e("NavitDrawObjects", "cancel draw has beed called. run == " + run);
		
		//this will be called before drawing a new selection, if old selection is still drawing, it is canceled

		
		run = false;

	}
	
	
	//no real layer sync implemented atm
	public void draw_to_screen(int mode) {
		

		if(mode == 1) {
			
			if(ng.in_map) {
					if(NavitDrawObjectsPool.thread_n != 1) {
						ng.draw_canvas.drawBitmap(priv_bitmap[idx], 0, 0, paint);
					}
					ng.zoomFactor = 1;
					ng.viewInvalidate();
				}
					
			run = false;
			
			//long duration =  android.os.SystemClock.currentThreadTimeMillis();
			
			long duration =  android.os.SystemClock.elapsedRealtime() - start;
			
			Log.e("NavitDrawObjects", "Drawing time (java): "  + duration + " ms (Thread " + thread_num + ")");
			
			
			//draw_obj_list.clear(); 

			this.interrupt();

			
		} else if(mode == 2) {
			//item done	
			
			
			
		} else if(mode == 3) {
			// layer done
			/*
			if(obj_since_drawn > 0) {
				//synchronized(this) {
					if(ng.in_map)
						ng.draw_canvas.drawBitmap(priv_bitmap[idx], 0, 0, paint);

					//clear image to ensure that we do not cover the lower layer from another thread
					priv_bitmap[idx].eraseColor(0);

					obj_since_drawn = -1;


				//}
				
			}*/
		}

	}
	
	
	public void add_object(NavitDrawObject obj) {
		
		if(draw_obj_list.offer(obj) == false) {
			
			Log.e("NavitDrawObjects", "failed to add object to queue");
		}
	}
	

	
	private void draw_polyline(Paint p, int c[])
	{
		int i, ndashes;
		float [] intervals;
		//Log.e("NavitDrawObjects","draw_polyline with " + c.length + " points");
		paint_polyline.setStrokeWidth(c[0]);
		paint_polyline.setARGB(c[1],c[2],c[3],c[4]);
		

		
		
		ndashes=c[5];
		intervals=new float[ndashes+(ndashes%2)];
		for (i = 0; i < ndashes; i++)
			intervals[i]=c[6+i];

		if((ndashes%2)==1)
			intervals[ndashes]=intervals[ndashes-1];
			
		if(ndashes>0)
			paint_polyline.setPathEffect(new android.graphics.DashPathEffect(intervals,0.0f));
			
		
		path.moveTo(c[6+ndashes], c[7+ndashes]);
		for (i = 8+ndashes; i < c.length; i += 2)
		{
			path.lineTo(c[i], c[i + 1]);
		}
		
		priv_canvas[idx].drawPath(path, paint_polyline);
		
		/*int offset = 6 + ndashes;
		int count = (c.length - offset);
		
		float[] cf;
		cf = new float[count];
		for(i=0;i<count;i++)
			cf[i] = (float) c[i+offset];
		
		
		priv_canvas[idx].drawLines(cf, 0, count, paint_polyline);*/
		
		paint_polyline.setPathEffect(null);
		
		path.rewind(); // uses structures again. reset() is slower
	}

	
	public void draw_polygon(Paint p, int c[])
	{
		//Log.e("NavitGraphics","draw_polygon with " + c.length + " points");
		paint_polygon.setStrokeWidth(c[0]);
		paint_polygon.setARGB(c[1],c[2],c[3],c[4]);
		

		
		path.moveTo(c[5], c[6]);
		for (int i = 7; i < c.length; i += 2)
		{
			path.lineTo(c[i], c[i + 1]);
		}
		
		priv_canvas[idx].drawPath(path, paint_polygon);
		path.rewind();
	}
	

	
	public void draw_rectangle(Paint paint, int x, int y, int w, int h)
	{
		//only do this when one thread is activ!!

		Rect r = new Rect(x, y, x + w, y + h);
		paint.setStyle(Paint.Style.FILL);
		paint.setAntiAlias(true);

		priv_canvas[idx].drawRect(r,paint);
	}

	
	public void draw_circle(Paint p, int x, int y, int r)
	{
		//Log.e("NavitGraphics","draw_circle");
		//		float fx = x;
		//		float fy = y;
		//		float fr = r / 2;
		paint.setStyle(Paint.Style.STROKE);
		priv_canvas[idx].drawCircle(x, y, r / 2, paint);
	}
	
	private Paint paint_text = new Paint();
	
	public void draw_text(int x, int y, String text, int size, int dx, int dy, int bgcolor, int lw, int fgcolor)
	{
		
		paint_text.setStrokeWidth(lw);
		
		Path path=null;
	
		paint_text.setTextSize(size / 15);
		paint_text.setStyle(Paint.Style.FILL);

		if (dx != 0x10000 || dy != 0) {
			path = new Path();
			path.moveTo(x, y);
			path.rLineTo(dx, dy);
			paint_text.setTextAlign(android.graphics.Paint.Align.LEFT);
		}

		if(bgcolor!=0) {
			paint_text.setStrokeWidth(3);
			paint_text.setColor(bgcolor);
			paint_text.setStyle(Paint.Style.STROKE);
			if(path==null) {
				priv_canvas[idx].drawText(text, x, y, paint_text);
			} else {
				priv_canvas[idx].drawTextOnPath(text, path, 0, 0, paint_text);
			}
			paint_text.setStyle(Paint.Style.FILL);
			
		}

		paint_text.setColor(fgcolor);
		
		if(path==null) {
			priv_canvas[idx].drawText(text, x, y, paint_text);
		} else {
			priv_canvas[idx].drawTextOnPath(text, path, 0, 0, paint_text);
		}
		paint_text.clearShadowLayer();
	}
	public void draw_image(Paint p, int x, int y, Bitmap bitmap)
	{
		//Log.e("NavitGraphics","draw_image");
		//		float fx = x;
		//		float fy = y;
		priv_canvas[idx].drawBitmap(bitmap, x, y, paint);
	}
	
	
}