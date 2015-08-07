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


/**
 * @brief This class defines an object which contains all the information for the drawing-process.
 *
 * @author Sascha Oedekoven (07/2015)
 */

public class NavitDrawObject
{
	public NavitObjectType type;
	
	
	public int		mode;
	
	//polyline, polygon
	public Paint 	paint;
	public int 		c[];
	
	//rectangle
	public int		x;
	public int		y;
	public int 		w;
	public int 		h;
	
	//circle
	public int		r;
	
	//text
	public String	text;
	public int		size;
	public int 		dx;
	public int 		dy;
	public int		bgcolor;
	public int		lw;
	public int		fgcolor;
	
	//image
	public Bitmap	bitmap;
	
	public NavitDrawObject() {
		
	}
	
}