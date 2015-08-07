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
import java.util.List;
import java.util.ArrayList;

/**
 * @brief This function manages the drawing threads and assigns the incoming objects to one of the threads.
 * The number of threads can be set in NavitGraphics.java or in the GUI.
 * 
 * @author Sascha Oedekoven (07/2015)
 */

public class NavitDrawObjectsPool 
{
	
	private int next_thread;
	private NavitGraphics ng;
	
	//Canvas which will be shown at the screen
	private Canvas	screen_canvas;
	
	private static Bitmap bufferBitmap;
	private static Canvas bufferCanvas;
	
	private List<NavitDrawObjects> drawThreads;
	
	public static int thread_n;
	
	private int w;
	private int h;
	
	public NavitDrawObjectsPool(NavitGraphics navitgraphics, int thread_count) {
		
		next_thread = 0;
		
		ng = navitgraphics;
		thread_n = thread_count;
		drawThreads = new ArrayList<NavitDrawObjects>();
		Log.e("NavitDrawObjectsPool", "obj created");
	}
	
	
	public void init(Canvas canvas, int width, int height) {
		Log.e("NavitDrawObjectsPool", "init");
		screen_canvas = canvas;
		w = width;
		h = height;
		
	}
	
	private void startThreads() {
		
		Log.e("NavitDrawObjectsPool", "start Threads");
		
		//liste als array und dann ueberpruefen ob thread bereits laeuft
		
		if(drawThreads.size() != thread_n ) {
			//close all threads and start new!
			
			for(int i = 0; i < drawThreads.size(); i++) {
				drawThreads.get(i).run = false;
				drawThreads.get(i).cancel_draw();
			}
			drawThreads.clear();
			
			
			for(int i =0; i < thread_n; i++) {
				NavitDrawObjects d = new NavitDrawObjects(screen_canvas, w, h, ng, (i+1));
				d.start();
				drawThreads.add(d);
			}	
			
			
		} 
	}
	
	
	//threads beenden, wenn nicht gebraucht
	public void cancel_draw() {
		
		Log.e("NavitDrawObjectsPool", "cancel draw");
		
		//clears list of objects directly
		
		//if(drawThreads.size() == thread_n) {
			//reuse threads
			
			for(int i =0; i < drawThreads.size() ; i++) {
				
				//drawThreads.get(i).run = false;
				drawThreads.get(i).cancel_draw();
				
				/*if(drawThreads.get(i).run == false) {
					drawThreads.clear();
					return;
				} else {
					
					drawThreads.get(i).cancel_draw();
				}*/
			}
		//}
		
		drawThreads.clear();
		next_thread = 0;
	}
	
	public void draw_to_screen(int mode) {
		
		if(drawThreads.size() == 0)
			return;
		
		if(mode == 2) {
			//next_thread = (next_thread + 1) % thread_n;
			
		} else {
		
			NavitDrawObject obj = new NavitDrawObject();
			obj.type = NavitObjectType.TOSCREEN;
			obj.mode = mode;
			for(int i =thread_n-1; i >= 0 ; i--) {
				drawThreads.get(i).add_object(obj);
			}
			
		}
		
	}
	
	public void add_polyline(Paint paint, int c[]) {
		
		if(drawThreads.size() == 0)
			return;

		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.POLYLINE;
		obj.c = c;
		obj.paint = paint;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
		
		
	}
	
	public void add_polygon(Paint paint, int c[]) {

		if(drawThreads.size() == 0)
			return;
		
		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.POLYGON;
		obj.c = c;
		obj.paint = paint;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
		
	}
	
	public void add_rectangle(Paint paint, int x, int y, int w, int h) {
		
		if(drawThreads.size() == 0)
			return;

		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.RECTANGLE;
		obj.paint = paint;
		obj.x = x;
		obj.y = y;
		obj.w = w;
		obj.h = h;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
	
		
	}
	
	public void setThread_n(int n) {
		thread_n = n;
		startThreads();
		
	}
	
	public void add_circle(Paint paint, int x, int y, int r) {

		if(drawThreads.size() == 0)
			return;
		
		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.CIRCLE;
		obj.paint = paint;
		obj.x = x;
		obj.y = y;
		obj.r = r;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
	}
	
	public void add_text(int x, int y, String text, int size, int dx, int dy, int bgcolor, int lw, int fgcolor) {

		if(drawThreads.size() == 0)
			return;
		
		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.TEXT;
		obj.x = x;
		obj.y = y;
		obj.text = text;
		obj.size = size;
		obj.dx = dx;
		obj.dy = dy;
		obj.bgcolor = bgcolor;
		obj.lw = lw;
		obj.fgcolor = fgcolor;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
	}
	
	
	public void add_image(Paint paint, int x, int y, Bitmap bitmap) {
		
		if(drawThreads.size() == 0)
			return;
		
		NavitDrawObject obj = new NavitDrawObject();
		
		obj.type = NavitObjectType.IMAGE;
		obj.paint = paint;
		obj.x = x;
		obj.y = y;
		obj.bitmap = bitmap;
		
		//drawThreads.get(next_thread).add_object(obj);
		drawThreads.get(next_thread++ % thread_n).add_object(obj);
	}
	
	
}