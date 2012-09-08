/***********************************************************************\
*
* $Revision: 800 $
* $Date: 2012-01-28 10:49:16 +0100 (Sat, 28 Jan 2012) $
* $Author: trupp $
* Contents: simple widgets functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.File;
import java.io.InputStream;
import java.net.URL;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Scanner;

// graphics
import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.custom.ScrolledComposite;
import org.eclipse.swt.custom.StyledText;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.FontData;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.swt.widgets.Sash;
import org.eclipse.swt.widgets.Scale;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Slider;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.TabItem;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Tree;
import org.eclipse.swt.widgets.TreeColumn;
import org.eclipse.swt.widgets.TreeItem;
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/** widget variable types
 */
enum WidgetVariableTypes
{
  BOOLEAN,
  LONG,
  DOUBLE,
  STRING,
  ENUMERATION,
  OBJECT,
};

/** widget variable
 */
class WidgetVariable
{
  private WidgetVariableTypes type;
  private boolean             b;
  private long                l;
  private double              d;
  private String              string;
  private String              enumeration[];
  private Object              object;

  /** create widget variable
   * @param b/l/d/string/enumeration value
   */
  WidgetVariable(boolean b)
  {
    this.type = WidgetVariableTypes.BOOLEAN;
    this.b    = b;
  }
  WidgetVariable(long l)
  {
    this.type = WidgetVariableTypes.LONG;
    this.l    = l;
  }
  WidgetVariable(double d)
  {
    this.type = WidgetVariableTypes.DOUBLE;
    this.d    = d;
  }
  WidgetVariable(String string)
  {
    this.type   = WidgetVariableTypes.STRING;
    this.string = string;
  }
  WidgetVariable(String enumeration[])
  {
    this.type        = WidgetVariableTypes.ENUMERATION;
    this.enumeration = enumeration;
  }
  WidgetVariable(Object object)
  {
    this.type   = WidgetVariableTypes.OBJECT;
    this.object = object;
  }

  /** get variable type
   * @return type
   */
  WidgetVariableTypes getType()
  {
    return type;
  }

  /** get boolean value
   * @return true or false
   */
  boolean getBoolean()
  {
    assert type == WidgetVariableTypes.BOOLEAN;

    return b;
  }

  /** get long value
   * @return value
   */
  long getLong()
  {
    assert type == WidgetVariableTypes.LONG;

    return l;
  }

  /** get double value
   * @return value
   */
  double getDouble()
  {
    assert type == WidgetVariableTypes.DOUBLE;

    return d;
  }

  /** get string value
   * @return value
   */
  String getString()
  {
    assert (type == WidgetVariableTypes.STRING) || (type == WidgetVariableTypes.ENUMERATION);

    return string;
  }

  /** set boolean value
   * @param b value
   * @return true iff changed
   */
  boolean set(boolean b)
  {
    boolean changedFlag;

    assert type == WidgetVariableTypes.BOOLEAN;

    changedFlag = (this.b != b);

    this.b = b;
    Widgets.modified(this);

    return changedFlag;
  }

  /** set long value
   * @param l value
   * @return true iff changed
   */
  boolean set(long l)
  {
    boolean changedFlag;

    assert type == WidgetVariableTypes.LONG;

    changedFlag = (this.l != l);

    this.l = l;
    Widgets.modified(this);

    return changedFlag;
  }

  /** set double value
   * @param d value
   * @return true iff changed
   */
  boolean set(double d)
  {
    boolean changedFlag;

    assert type == WidgetVariableTypes.DOUBLE;

    changedFlag = (this.d != d);

    this.d = d;
    Widgets.modified(this);

    return changedFlag;
  }

  /** set string value
   * @param string value
   * @return true iff changed
   */
  boolean set(String string)
  {
    boolean changedFlag = false;

    assert (type == WidgetVariableTypes.STRING) || (type == WidgetVariableTypes.ENUMERATION);

    switch (type)
    {
      case STRING:
        changedFlag = (this.string != string);

        this.string = string;
        Widgets.modified(this);
        break;
      case ENUMERATION:
        for (String s : enumeration)
        {
          if (s.equals(string))
          {
            changedFlag = (this.string != string);

            this.string = string;
            Widgets.modified(this);
            break;
          }
        }
        break;
    }

    return changedFlag;
  }

  /** compare string values
   * @param value value to compare with
   * @return true iff equal
   */
  public boolean equals(String value)
  {
    String s = toString();
    return (s != null) ? s.equals(value) : (value == null);
  }

  /** compare object reference
   * @param object object  to compare with
   * @return true iff equal
   */
  public boolean equals(Object object)
  {
    return this.object == object;
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    switch (type)
    {
      case BOOLEAN:     return Boolean.toString(b);
      case LONG:        return Long.toString(l);
      case DOUBLE:      return Double.toString(d);
      case STRING:      return string;
      case ENUMERATION: return string;
      case OBJECT:      return object.toString();
    }
    return "";
  }
}

/** widget modify listener
 */
class WidgetModifyListener
{
  private Widget           widget;
  private WidgetVariable[] variables;

  // cached text for widget
  private String cachedText = null;

  /** create widget listener
   */
  WidgetModifyListener()
  {
    this.widget    = null;
    this.variables = null;
  }

  /** create widget listener
   * @param widget widget
   * @param variable widget variable
   */
  WidgetModifyListener(Widget widget, WidgetVariable variable)
  {
    this(widget,new WidgetVariable[]{variable});
  }

  /** create widget listener
   * @param widget widget
   * @param variable widget variable
   */
  WidgetModifyListener(Widget widget, WidgetVariable[] variables)
  {
    this.widget    = widget;
    this.variables = variables;
  }

  /** create widget listener
   * @param widget widget
   * @param object object
   */
  WidgetModifyListener(Widget widget, Object object)
  {
    this(widget,new WidgetVariable(object));
  }

  /** create widget listener
   * @param widget widget
   * @param objects objects
   */
  WidgetModifyListener(Widget widget, Object[] objects)
  {
    this.widget    = widget;
    this.variables = new WidgetVariable[objects.length];
    for (int i = 0; i < objects.length; i++)
    {
      this.variables[i] = new WidgetVariable(objects[i]);
    }
  }

  /** set widget
   * @param widget widget
   */
  void setControl(Widget widget)
  {
    this.widget = widget;
  }

  /** set variable
   * @param variable widget variable
   * @return
   */
  void setVariable(WidgetVariable variable)
  {
    this.variables = new WidgetVariable[]{variable};
  }

  /** compare variables
   * @param object variable object
   * @return true iff equal variable object is equal to some variable
   */
  public boolean equals(Object object)
  {
    for (WidgetVariable variable : variables)
    {
      if (variable != null)
      {
        switch (variable.getType())
        {
          case BOOLEAN:
          case LONG:
          case DOUBLE:
          case STRING:
          case ENUMERATION:
            if (variable.equals(object.toString())) return true;
            break;
          case OBJECT:
            if (variable.equals(object)) return true;
            break;
        }
      }
    }

    return false;
  }

  /** set text or selection fo widget according to value of variable
   * @param widget widget widget to set
   * @param variable variable
   */
  void modified(Widget widget, WidgetVariable variable)
  {
    if      (widget instanceof Label)
    {
      Label widgetLabel = (Label)widget;

      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break;
          case DOUBLE: text = Double.toString(variable.getDouble()); break;
          case STRING: text = variable.getString(); break;
        }
      }
      if ((text != null) && !text.equals(cachedText))
      {
        widgetLabel.setText(text);
// label layout does not work as expected: width of first label is expanded, rest reduced?
//        widgetLabel.getParent().layout();
        cachedText = text;
      }
    }
    else if (widget instanceof Button)
    {
      Button widgetButton = (Button)widget;

      if      ((widgetButton.getStyle() & SWT.PUSH) == SWT.PUSH)
      {
        String text = getString(variable);
        if (text == null)
        {
          switch (variable.getType())
          {
            case LONG:   text = Long.toString(variable.getLong()); break;
            case DOUBLE: text = Double.toString(variable.getDouble()); break;
            case STRING: text = variable.getString(); break;
          }
        }
        if ((text != null) && !text.equals(cachedText))
        {
          widgetButton.setText(text);
          widgetButton.getParent().layout();
          cachedText = text;
        }
      }
      else if ((widgetButton.getStyle() & SWT.CHECK) == SWT.CHECK)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break;
          case DOUBLE:  selection = (variable.getDouble() != 0); break;
        }
        widgetButton.setSelection(selection);
      }
      else if ((widgetButton.getStyle() & SWT.RADIO) == SWT.RADIO)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break;
          case DOUBLE:  selection = (variable.getDouble() != 0); break;
        }
        widgetButton.setSelection(selection);
      }
    }
    else if (widget instanceof Combo)
    {
      Combo widgetCombo = (Combo)widget;

      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case BOOLEAN:     text = Boolean.toString(variable.getBoolean()); break;
          case LONG:        text = Long.toString(variable.getLong()); break;
          case DOUBLE:      text = Double.toString(variable.getDouble()); break;
          case STRING:      text = variable.getString(); break;
          case ENUMERATION: text = variable.getString(); break;
        }
      }
      if ((text != null) && !text.equals(cachedText))
      {
        widgetCombo.setText(text);
        widgetCombo.getParent().layout();
        cachedText = text;
      }
    }
    else if (widget instanceof Text)
    {
      Text widgetText = (Text)widget;

      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break;
          case DOUBLE: text = Double.toString(variable.getDouble()); break;
          case STRING: text = variable.getString(); break;
        }
      }
      if ((text != null) && !text.equals(cachedText))
      {
        widgetText.setText(text);
// text layout does not work as expected: width of first label is expanded, rest reduced?
//        widgetText.getParent().layout();
        cachedText = text;
      }
    }
    else if (widget instanceof StyledText)
    {
      StyledText widgetStyledText = (StyledText)widget;

      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break;
          case DOUBLE: text = Double.toString(variable.getDouble()); break;
          case STRING: text = variable.getString(); break;
        }
      }
      if ((text != null) && !text.equals(cachedText))
      {
        widgetStyledText.setText(text);
        widgetStyledText.getParent().layout();
        cachedText = text;
      }
    }
    else if (widget instanceof Spinner)
    {
      Spinner widgetSpinner = (Spinner)widget;

      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break;
        case DOUBLE: n = (int)variable.getDouble(); break;
      }
      widgetSpinner.setSelection(n);
    }
    else if (widget instanceof Slider)
    {
      Slider widgetSlider = (Slider)widget;

      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break;
        case DOUBLE: n = (int)variable.getDouble(); break;
      }
      widgetSlider.setSelection(n);
    }
    else if (widget instanceof Scale)
    {
      Scale widgetScale = (Scale)widget;

      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break;
        case DOUBLE: n = (int)variable.getDouble(); break;
      }
      widgetScale.setSelection(n);
    }
    else if (widget instanceof ProgressBar)
    {
      ProgressBar widgetProgressBar = (ProgressBar)widget;

      double value = 0;
      switch (variable.getType())
      {
        case LONG:   value = (double)variable.getLong(); break;
        case DOUBLE: value = variable.getDouble(); break;
      }
      widgetProgressBar.setSelection(value);
    }
    else if (widget instanceof MenuItem)
    {
      MenuItem widgetMenuItem = (MenuItem)widget;
    }
    else
    {
      throw new InternalError("Unhandled widget '"+widget+"' in widget listener!");
    }
  }

  /** modified handler
   * Note: required because it can be overwritten by specific handler
   * @param control control to notify about modified variable
   * @param variable variable
   */
  void modified(Control control, WidgetVariable variable)
  {
    modified((Widget)control,variable);
  }

  /** modified handler
   * Note: required because it can be overwritten by specific handler
   * @param menuItem menu item to notify about modified variable
   * @param variable variable
   */
  void modified(MenuItem menuItem, WidgetVariable variable)
  {
    modified((Widget)menuItem,variable);
  }

  /** set text or selection fo widget according to value of variable
   * @param widget widget to notify about modified variable
   * @param variables variables
   */
  void modified(Widget widget, WidgetVariable[] variables)
  {
    for (WidgetVariable variable : variables)
    {
      modified(widget,variable);
    }
  }

  /** set text or selection fo widget according to value of variable
   * @param control control to notify about modified variable
   * @param variables variables
   */
  void modified(Control control, WidgetVariable[] variables)
  {
    for (WidgetVariable variable : variables)
    {
      modified(control,variable);
    }
  }

  /** set text or selection fo widget according to value of variable
   * @param menuItem menu item to notify about modified variable
   * @param variables variables
   */
  void modified(MenuItem menuItem, WidgetVariable[] variables)
  {
    modified((Widget)menuItem,variables);
  }

  /** notify modify variable
   * @param variable widget variable
   */
  public void modified(WidgetVariable variable)
  {
    if (widget instanceof Control)
    {
      modified((Control)widget,variable);
    }
    else
    {
      modified(widget,variable);
    }
  }

  /** notify modify variable
   * @param variables widget variables
   */
  public void modified(WidgetVariable[] variables)
  {
    if (widget instanceof Control)
    {
      modified((Control)widget,variables);
    }
    else
    {
      modified(widget,variables);
    }
  }

  /** notify modify variable
   * @param widget widget to notify
   */
  void modified(Widget widget)
  {
    modified(widget,variables);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param control control to notify
   */
  void modified(Control control)
  {
    modified(control,variables);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param button button to notify
   */
  void modified(Combo combo)
  {
    modified((Control)combo);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param button button to notify
   */
  void modified(Button button)
  {
    modified((Control)button);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param menuItem menu item to notify
   */
  void modified(MenuItem menuItem)
  {
    modified(menuItem,variables);
  }

  /** notify modify variable
   */
  public void modified()
  {
    if (widget instanceof Control)
    {
      modified((Control)widget);
    }
    else if (widget instanceof MenuItem)
    {
      modified((MenuItem)widget);
    }
    else
    {
      modified(widget);
    }
  }

  /** get string of variable
   * @param variable widget variable
   */
  String getString(WidgetVariable variable)
  {
    return null;
  }
}

/** widget event
 */
class WidgetEvent
{
  private HashSet<WidgetEventListener> widgetEventListenerSet;

  /** create widget event listener
   */
  WidgetEvent()
  {
    widgetEventListenerSet = new HashSet<WidgetEventListener>();
  }

  /** add widget event listern
   * @param widgetEventListener widget event listern to add

   */
  public void add(WidgetEventListener widgetEventListener)
  {
    widgetEventListenerSet.add(widgetEventListener);
  }

  /** remove widget event listern
   * @param widgetEventListener widget event listern to remove

   */
  public void remove(WidgetEventListener widgetEventListener)
  {
    widgetEventListenerSet.remove(widgetEventListener);
  }

  /** trigger widget event
   */
  public void trigger()
  {
    for (WidgetEventListener widgetEventListener : widgetEventListenerSet)
    {
      widgetEventListener.trigger();
    }
  }
}

/** widget event listener
 */
class WidgetEventListener
{
  private Control     control;
  private Widget      widget;
  private WidgetEvent widgetEvent;

  /** create widget listener
   * @param control control widget
   * @param widgetEvent widget event
   */
  WidgetEventListener(Widget widget, WidgetEvent widgetEvent)
  {
    this.widget      = widget;
    this.widgetEvent = widgetEvent;
  }

  /** add widget event listern
   * @param widgetEventListener widget event listern to add
   */
  public void add()
  {
    widgetEvent.add(this);
  }

  /** remove widget event listern
   * @param widgetEventListener widget event listern to remove
   */
  public void remove()
  {
    widgetEvent.remove(this);
  }

  /** trigger handler
   * @param widget widget
   */
  public void trigger(Widget widget)
  {
  }

  /** trigger handler
   * @param control control
   */
  public void trigger(Control control)
  {
  }

  /** trigger handler
   * @param menuItem menu item
   */
  public void trigger(MenuItem menuItem)
  {
  }

  /** trigger widget event
   */
  void trigger()
  {
    if      (widget instanceof Control)
    {
      trigger((Control)widget);
    }
    else if (widget instanceof MenuItem)
    {
      trigger((MenuItem)widget);
    }
    trigger(widget);
  }
}

class Widgets
{
  //-----------------------------------------------------------------------

  /** list of widgets listeners
   */
  private static ArrayList<WidgetModifyListener> listenersList = new ArrayList<WidgetModifyListener>();

  //-----------------------------------------------------------------------

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param width,height width/height
   * @param minWidth,minHeight min. width/height
   * @param maxWidth,maxHeight max. width/height
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight, int maxWidth, int maxHeight)
  {
    TableLayoutData tableLayoutData = new TableLayoutData(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,minWidth,minHeight,maxWidth,maxHeight);
    control.setLayoutData(tableLayoutData);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param width,height width/height
   * @param minWidth,minHeight min. width/height
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,minWidth,minHeight,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param width,height min. width/height
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param size min. width/height
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param pad padding X/Y
   * @param size min. width/height
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point pad, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad.x,pad.y,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param size padding size
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param pad padding X/Y
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad,pad);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   */
  public static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,0);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   */
  public static void layout(Control control, int row, int column, int style)
  {
//    layout(control,row,column,style,0,0);
    layout(control,row,column,style,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** get text size
   * @param control control
   * @return size of text
   */
  public static Point getTextSize(GC gc, String text)
  {
    return gc.textExtent(text);
  }

  /** get text size
   * @param control control
   * @return size of text
   */
  public static Point getTextSize(Control control, String text)
  {
    Point size;

    GC gc = new GC(control);
    size = getTextSize(gc,text);
    gc.dispose();

    return size;
  }

  /** get max. text size
   * @param control control
   * @return max. size of all texts
   */
  public static Point getTextSize(GC gc, String[] texts)
  {
    Point size;
    Point point;

    size = new Point(0,0);
    for (String text : texts)
    {
      point = getTextSize(gc,text);
      size.x = Math.max(size.x,point.x);
      size.y = Math.max(size.y,point.y);
    }

    return size;
  }

  /** get max. text size
   * @param control control
   * @return max. size of all texts
   */
  public static Point getTextSize(Control control, String[] texts)
  {
    Point size;

    GC gc = new GC(control);
    size = getTextSize(gc,texts);
    gc.dispose();

    return size;
  }

  /** get text width
   * @param control control
   * @return width of text
   */
  public static int getTextWidth(GC gc, String text)
  {
    return getTextSize(gc,text).x;
  }

  /** get text width
   * @param control control
   * @return width of text
   */
  public static int getTextWidth(Control control, String text)
  {
    return getTextSize(control,text).x;
  }

  /** get max. text width
   * @param control control
   * @return max. width of all texts
   */
  public static int getTextWidth(GC gc, String[] texts)
  {
    return getTextSize(gc,texts).x;
  }

  /** get max. text width
   * @param control control
   * @return max. width of all texts
   */
  public static int getTextWidth(Control control, String[] texts)
  {
    return getTextSize(control,texts).x;
  }

  /** get text height
   * @param control control
   * @return height of text
   */
  public static int getTextHeight(GC gc)
  {
// NYI: does not work?
//    return gc.getFontMetrics().getHeight();
    return gc.textExtent("Hj").y;
  }

  /** get text height
   * @param control control
   * @return height of text
   */
  public static int getTextHeight(Control control)
  {
    int height;

    GC gc = new GC(control);
    height = getTextHeight(gc);
    gc.dispose();

    return height;
  }

  /** load image from jar or directory "images"
   * @param fileName image file name
   * @return image
   */
  public static Image loadImage(Display display, String fileName)
  {
    // try to load from jar file
    try
    {
      InputStream inputStream = display.getClass().getClassLoader().getResourceAsStream("images/"+fileName);
      Image image = new Image(display,inputStream);
      inputStream.close();

      return image;
    }
    catch (Exception exception)
    {
      // ignored
    }

    // try to load from file
    return new Image(display,"images"+File.separator+fileName);
  }

  /** create new font
   * @param display display
   * @param fontData font data
   * @return font or null
   */
  public static Font newFont(Display display, FontData fontData)
  {
    return (fontData != null) ? new Font(display,fontData) : null;
  }

  /** create new font
   * @param display display
   * @param name font name
   * @param height font height
   * @param style font style
   * @return font or null
   */
  public static Font newFont(Display display, String name, int height, int style)
  {
    return (name != null) ? new Font(display,name,height,style) : null;
  }

  /** create new font
   * @param display display
   * @param name font name
   * @param height font height
   * @return font or null
   */
  public static Font newFont(Display display, String name, int height)
  {
    return newFont(display,
                   name,
                   height,
                   display.getSystemFont().getFontData()[0].getStyle()
                  );
  }

  /** create new font
   * @param display display
   * @param name font name
   * @return font or null
   */
  public static Font newFont(Display display, String name)
  {
    return newFont(display,
                   name,
                   display.getSystemFont().getFontData()[0].getHeight()
                  );
  }

  /** convert font data to text
   * @param fontData font data
   * @return text
   */
  public static String fontDataToText(FontData fontData)
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append(fontData.getName());
    buffer.append(',');
    buffer.append(Integer.toString(fontData.getHeight()));
    int style = fontData.getStyle();
    if      ((style & (SWT.BOLD|SWT.ITALIC)) == (SWT.BOLD|SWT.ITALIC)) buffer.append(",bold italic");
    else if ((style & SWT.BOLD             ) == SWT.BOLD             ) buffer.append(",bold");
    else if ((style & SWT.ITALIC           ) == SWT.ITALIC           ) buffer.append(",italic");

    return buffer.toString();
  }

  /** convert text to font data
   * @param string string
   * @return font data or null
   */
  public static FontData textToFontData(String string)
  {
    String name;
    int    height = 0;
    int    style  = 0;

    Scanner scanner = new Scanner(string.trim()).useDelimiter("\\s*,\\s*");

    if (!scanner.hasNext()) return null;
    name = scanner.next();

    if (scanner.hasNext())
    {
      if (!scanner.hasNextInt()) return null;
      height = scanner.nextInt();
    }

    if (scanner.hasNext())
    {
      String styleString = scanner.next();
      if      (styleString.equalsIgnoreCase("normal"     )) style = SWT.NORMAL;
      else if (styleString.equalsIgnoreCase("bold italic")) style = SWT.BOLD|SWT.ITALIC;
      else if (styleString.equalsIgnoreCase("bold"       )) style = SWT.BOLD;
      else if (styleString.equalsIgnoreCase("italic"     )) style = SWT.ITALIC;
      else return null;
    }

    return new FontData(name,height,style);
  }

  /** get key code text
   * @param keyCode accelerator key code
   * @param separatorText separator text
   * @param controlText Ctrl key text or null
   * @param altText ALT key text or null
   * @param controlText Shift key text or null
   * @return accelerator key code text
   */
  private static String acceleratorToText(int accelerator, String separatorText, String controlText, String altText, String shiftText)
  {
    StringBuilder buffer = new StringBuilder();

    if (accelerator != SWT.NONE)
    {
      if (   ((accelerator & SWT.MOD1) == SWT.SHIFT)
          || ((accelerator & SWT.MOD2) == SWT.SHIFT)
          || ((accelerator & SWT.MOD3) == SWT.SHIFT)
          || ((accelerator & SWT.MOD4) == SWT.SHIFT)
         )
      {
        if (buffer.length() > 0) buffer.append('+'); buffer.append((shiftText   != null)?shiftText  :"Shift");
      }
      if (   ((accelerator & SWT.MOD1) == SWT.CTRL)
          || ((accelerator & SWT.MOD2) == SWT.CTRL)
          || ((accelerator & SWT.MOD3) == SWT.CTRL)
          || ((accelerator & SWT.MOD4) == SWT.CTRL)
         )
      {
        if (buffer.length() > 0) buffer.append('+'); buffer.append((controlText != null)?controlText:"Ctrl" );
      }
      if (   ((accelerator & SWT.MOD1) == SWT.ALT)
          || ((accelerator & SWT.MOD2) == SWT.ALT)
          || ((accelerator & SWT.MOD3) == SWT.ALT)
          || ((accelerator & SWT.MOD4) == SWT.ALT)
         )
      {
        if (buffer.length() > 0) buffer.append('+'); buffer.append((altText     != null)?altText    :"Alt"  );
      }

      if ((separatorText != null) && (buffer.length() > 0)) buffer.append(separatorText);
      if      ((accelerator & SWT.KEY_MASK) == SWT.F1         ) buffer.append("F1");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F2         ) buffer.append("F2");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F3         ) buffer.append("F3");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F4         ) buffer.append("F4");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F5         ) buffer.append("F5");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F6         ) buffer.append("F6");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F7         ) buffer.append("F7");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F8         ) buffer.append("F8");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F9         ) buffer.append("F9");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F10        ) buffer.append("F10");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F11        ) buffer.append("F11");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F12        ) buffer.append("F12");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F13        ) buffer.append("F13");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F14        ) buffer.append("F14");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F15        ) buffer.append("F15");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F16        ) buffer.append("F16");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F17        ) buffer.append("F17");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F18        ) buffer.append("F18");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F19        ) buffer.append("F19");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F20        ) buffer.append("F20");
      else if ((accelerator & SWT.KEY_MASK) == SWT.CR         ) buffer.append("Return");
      else if ((accelerator & SWT.KEY_MASK) == SWT.TAB        ) buffer.append("Tab");
      else if ((accelerator & SWT.KEY_MASK) == SWT.ARROW_LEFT ) buffer.append("Left");
      else if ((accelerator & SWT.KEY_MASK) == SWT.ARROW_RIGHT) buffer.append("Right");
      else if ((accelerator & SWT.KEY_MASK) == SWT.ARROW_UP   ) buffer.append("Up");
      else if ((accelerator & SWT.KEY_MASK) == SWT.ARROW_DOWN ) buffer.append("Down");
      else if ((accelerator & SWT.KEY_MASK) == SWT.PAGE_UP    ) buffer.append("PageUp");
      else if ((accelerator & SWT.KEY_MASK) == SWT.PAGE_DOWN  ) buffer.append("PageDown");
      else if ((accelerator & SWT.KEY_MASK) == SWT.BS         ) buffer.append("Backspace");
      else if ((accelerator & SWT.KEY_MASK) == SWT.DEL        ) buffer.append("Delete");
      else if ((accelerator & SWT.KEY_MASK) == SWT.INSERT     ) buffer.append("Insert");
      else if ((accelerator & SWT.KEY_MASK) == SWT.HOME       ) buffer.append("Home");
      else if ((accelerator & SWT.KEY_MASK) == SWT.END        ) buffer.append("End");
      else if ((accelerator & SWT.KEY_MASK) == SWT.ESC        ) buffer.append("ESC");
      else if ((accelerator & SWT.KEY_MASK) == ' '            ) buffer.append("Space");
      else                                                      buffer.append(Character.toUpperCase((char)(accelerator & SWT.KEY_MASK)));
    }

    return buffer.toString();
  }

  /** get accelerator key code text
   * @param keyCode accelerator key code
   * @return accelerator key code text
   */
  public static String acceleratorToText(int accelerator)
  {
    return acceleratorToText(accelerator,"+","Ctrl","Alt","Shift");
  }

  /** get accelerator key code text for menu item
   * @param keyCode accelerator key code
   * @return accelerator key code text
   */
  public static String menuAcceleratorToText(int accelerator)
  {
    return acceleratorToText(accelerator);
  }

  /** get accelerator key code text for button
   * @param keyCode accelerator key code
   * @return accelerator key code text
   */
  public static String buttonAcceleratorToText(int accelerator)
  {
    return acceleratorToText(accelerator,null,"^","\u8657","\u9830");
  }

  /** convert text to key accelerator
   * @param text accelerator key code text
   * @return accelerator key code
   */
  public static int textToAccelerator(String text)
  {
    int keyCode = 0;

    text = text.toLowerCase();

    // parse modifiers
    boolean modifierFlag = true;
    do
    {
      if      (text.startsWith("ctrl+"))
      {
        keyCode |= SWT.CTRL;
        text = text.substring(5);
      }
      else if (text.startsWith("alt+"))
      {
        keyCode |= SWT.ALT;
        text = text.substring(4);
      }
      else if (text.startsWith("shift+"))
      {
        keyCode |= SWT.SHIFT;
        text = text.substring(6);
      }
      else
      {
        modifierFlag = false;
      }
    }
    while (modifierFlag);

    // parse character/key
    if      (text.equals("f1"            )) keyCode |= SWT.F1;
    else if (text.equals("f2"            )) keyCode |= SWT.F2;
    else if (text.equals("f3"            )) keyCode |= SWT.F3;
    else if (text.equals("f4"            )) keyCode |= SWT.F4;
    else if (text.equals("f5"            )) keyCode |= SWT.F5;
    else if (text.equals("f6"            )) keyCode |= SWT.F6;
    else if (text.equals("f7"            )) keyCode |= SWT.F7;
    else if (text.equals("f8"            )) keyCode |= SWT.F8;
    else if (text.equals("f9"            )) keyCode |= SWT.F9;
    else if (text.equals("f10"           )) keyCode |= SWT.F10;
    else if (text.equals("f11"           )) keyCode |= SWT.F11;
    else if (text.equals("f12"           )) keyCode |= SWT.F12;
    else if (text.equals("f13"           )) keyCode |= SWT.F13;
    else if (text.equals("f14"           )) keyCode |= SWT.F14;
    else if (text.equals("f15"           )) keyCode |= SWT.F15;
    else if (text.equals("f16"           )) keyCode |= SWT.F16;
    else if (text.equals("f17"           )) keyCode |= SWT.F17;
    else if (text.equals("f18"           )) keyCode |= SWT.F18;
    else if (text.equals("f19"           )) keyCode |= SWT.F19;
    else if (text.equals("f20"           )) keyCode |= SWT.F20;
    else if (text.equals("keypad0"       )) keyCode |= SWT.KEYPAD_0;
    else if (text.equals("keypad1"       )) keyCode |= SWT.KEYPAD_1;
    else if (text.equals("keypad2"       )) keyCode |= SWT.KEYPAD_2;
    else if (text.equals("keypad3"       )) keyCode |= SWT.KEYPAD_3;
    else if (text.equals("keypad4"       )) keyCode |= SWT.KEYPAD_4;
    else if (text.equals("keypad5"       )) keyCode |= SWT.KEYPAD_5;
    else if (text.equals("keypad6"       )) keyCode |= SWT.KEYPAD_6;
    else if (text.equals("keypad7"       )) keyCode |= SWT.KEYPAD_7;
    else if (text.equals("keypad8"       )) keyCode |= SWT.KEYPAD_8;
    else if (text.equals("keypad9"       )) keyCode |= SWT.KEYPAD_9;
    else if (text.equals("keypadplus"    )) keyCode |= SWT.KEYPAD_ADD;
    else if (text.equals("keypadminus"   )) keyCode |= SWT.KEYPAD_SUBTRACT;
    else if (text.equals("keypadmultiply")) keyCode |= SWT.KEYPAD_MULTIPLY;
    else if (text.equals("keypaddivide"  )) keyCode |= SWT.KEYPAD_DIVIDE;
    else if (text.equals("keypaddecimal" )) keyCode |= SWT.KEYPAD_DECIMAL;
    else if (text.equals("return"        )) keyCode |= SWT.CR;
    else if (text.equals("enter"         )) keyCode |= SWT.KEYPAD_CR;
    else if (text.equals("tab"           )) keyCode |= SWT.TAB;
    else if (text.equals("left"          )) keyCode |= SWT.ARROW_LEFT;
    else if (text.equals("right"         )) keyCode |= SWT.ARROW_RIGHT;
    else if (text.equals("up"            )) keyCode |= SWT.ARROW_UP;
    else if (text.equals("down"          )) keyCode |= SWT.ARROW_DOWN;
    else if (text.equals("pageup"        )) keyCode |= SWT.PAGE_UP;
    else if (text.equals("pagedown"      )) keyCode |= SWT.PAGE_DOWN;
    else if (text.equals("bs"            )) keyCode |= SWT.BS;
    else if (text.equals("delete"        )) keyCode |= SWT.DEL;
    else if (text.equals("insert"        )) keyCode |= SWT.INSERT;
    else if (text.equals("home"          )) keyCode |= SWT.HOME;
    else if (text.equals("end"           )) keyCode |= SWT.END;
    else if (text.equals("esc"           )) keyCode |= SWT.ESC;
    else if (text.equals("space"         )) keyCode |= ' ';
    else if (text.length() == 1)            keyCode |= (int)text.charAt(0);
    else                                    keyCode = 0;

    return keyCode;
  }

  /** check if key event is accelerator key
   * @param keyCode key code
   * @param stateMask event state mask
   * @param character character
   * @param accelerator accelerator key code or SWT.NONE
   * @return true iff key event is accelerator key
   */
  public static boolean isAccelerator(int keyCode, int stateMask, char character, int accelerator)
  {
    if (accelerator != SWT.NONE)
    {
      if      (keyCode == SWT.KEYPAD_ADD     ) keyCode = '+';
      else if (keyCode == SWT.KEYPAD_SUBTRACT) keyCode = '-';
      else if (keyCode == SWT.KEYPAD_MULTIPLY) keyCode = '*';
      else if (keyCode == SWT.KEYPAD_DIVIDE  ) keyCode = '/';
      else if (keyCode == SWT.KEYPAD_EQUAL   ) keyCode = '=';
      else if (keyCode == SWT.KEYPAD_CR      ) keyCode = SWT.CR;

      if ((accelerator & SWT.MODIFIER_MASK) != 0)
      {
        // accelerator has a modified -> match modifier+key code
        return    (((accelerator & SWT.MODIFIER_MASK) & stateMask) == (accelerator & SWT.MODIFIER_MASK))
               && ((accelerator & SWT.KEY_MASK) == keyCode);
      }
      else
      {
        // accelerator has no modified -> accept no modifier/shift+character or match key code
        return    (   (   ((stateMask & SWT.MODIFIER_MASK) == 0)
                       || ((stateMask & SWT.MODIFIER_MASK) == SWT.SHIFT)
                      )
                   && ((accelerator & SWT.KEY_MASK) == character)
                  )
               || ((accelerator & SWT.KEY_MASK) == keyCode);
      }
    }
    else
    {
      return false;
    }
  }

  /** check if key event is accelerator key
   * @param keyEvent key event
   * @param accelerator accelerator key code or SWT.NONE
   * @return true iff key event is accelerator key
   */
  public static boolean isAccelerator(KeyEvent keyEvent, int accelerator)
  {
    return isAccelerator(keyEvent.keyCode,keyEvent.stateMask,keyEvent.character,accelerator);
  }

  /** check if key event is accelerator key
   * @param event event
   * @param accelerator accelerator key code or SWT.NONE
   * @return true iff event is accelerator key
   */
  public static boolean isAccelerator(Event event, int accelerator)
  {
    return isAccelerator(event.keyCode,event.stateMask,event.character,accelerator);
  }

  /** set enabled
   * @param control control to enable/disable
   * @param enableFlag true to enable, false to disable
   */
  public static void setEnabled(Control control, boolean enableFlag)
  {
    if (!control.isDisposed())
    {
      control.setEnabled(enableFlag);
      control.getDisplay().update();
    }
  }

  /** set enabled
   * @param widget control/menu item to enable/disable
   * @param enableFlag true to enable, false to disable
   */
  static void setEnabled(Widget widget, boolean enableFlag)
  {
    if      (widget instanceof Control)
    {
      ((Control)widget).setEnabled(enableFlag);
    }
    else if (widget instanceof MenuItem)
    {
      ((MenuItem)widget).setEnabled(enableFlag);
    }
  }

  /** set visible
   * @param control control to make visible/invisible
   * @param visibleFlag true to make visible, false to make invisible
   */
  public static void setVisible(Control control, boolean visibleFlag)
  {
    if (!control.isDisposed())
    {
      TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
      tableLayoutData.hidden = !visibleFlag;
      control.setVisible(visibleFlag);
      if (visibleFlag)
      {
        control.getParent().layout();
      }
    }
  }

  /** set focus
   * @param control control to set focus
   */
  public static void setFocus(Control control)
  {
    if (!control.isDisposed())
    {
      control.setFocus();
      if      (control instanceof Text)
      {
        Text   widget = (Text)control;
        String text   = widget.getText();
        widget.setSelection(text.length(),text.length());
      }
      else if (control instanceof StyledText)
      {
        StyledText widget = (StyledText)control;
        String     text   = widget.getText();
        widget.setSelection(text.length(),text.length());
      }
      else if (control instanceof Combo)
      {
        Combo  widget = (Combo)control;

/* do not work correct?
        // no setSelection(n,m)-method, thus send key "end" event
        Event event   = new Event();
        event.type    = SWT.KeyDown;
        event.widget  = widget;
        event.keyCode = SWT.END;
        Display display = widget.getDisplay();
        display.post(event);
        display.update();
*/
      }
    }
  }

  /** set next focus for control
   * @param control control
   * @param nextControl next control to focus on RETURN
   */
  public static void setNextFocus(Control control, final Control nextControl)
  {
    if (!control.isDisposed())
    {
      SelectionListener selectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Widgets.setFocus(nextControl);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      };

      if      (control instanceof Button)
      {
        ((Button)control).addSelectionListener(selectionListener);
      }
      else if (control instanceof Combo)
      {
        ((Combo)control).addSelectionListener(selectionListener);
      }
      else if (control instanceof Spinner)
      {
        ((Spinner)control).addSelectionListener(selectionListener);
      }
      else if (control instanceof Text)
      {
        ((Text)control).addSelectionListener(selectionListener);
      }
      else
      {
        throw new Error("Internal error: unknown control");
      }
    }
  }

  /** set cursor for control
   * @param control control
   * @param cursor cursor to set or null
   */
  public static void setCursor(final Control control, final Cursor cursor)
  {
    if (!control.isDisposed())
    {
      control.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          control.setCursor(cursor);
        }
      });
    }
  }

    /** resset cursor for control to default cursor
   * @param control control
   */
  public static void resetCursor(Control control)
  {
    setCursor(control,null);
  }

  /** set field in data structure
   * @param data data structure
   * @param field field name
   * @param value value
   */
  private static void setField(Object data, String field, Object value)
  {
    if (data != null)
    {
      try
      {
        data.getClass().getField(field).set(data,value);
      }
      catch (NoSuchFieldException exception)
      {
        throw new Error("INTERNAL ERROR: field access '"+exception.getMessage()+"'");
      }
      catch (IllegalAccessException exception)
      {
        throw new Error("INTERNAL ERROR: field access '"+exception.getMessage()+"'");
      }
    }
  }

  /** get field from data structure
   * @param data data structure
   * @param field field name
   * @return value or null
   */
  private static Object getField(Object data, String field)
  {
    Object value = null;

    if (data != null)
    {
      try
      {
        value = data.getClass().getField(field).get(data);
      }
      catch (NoSuchFieldException exception)
      {
        throw new Error("INTERNAL ERROR: field access '"+exception.getMessage()+"'");
      }
      catch (IllegalAccessException exception)
      {
        throw new Error("INTERNAL ERROR: field access '"+exception.getMessage()+"'");
      }
    }

    return value;
  }

  //-----------------------------------------------------------------------

  /** invoke button
   * @param button button to invoke
   */
  public static void invoke(Button button)
  {
    if (!button.isDisposed() && button.isEnabled())
    {
      if ((button.getStyle() & SWT.CHECK) != 0)
      {
        button.setSelection(!button.getSelection());
      }
      else
      {
        Event event = new Event();
        event.widget = button;

        button.notifyListeners(SWT.Selection,event);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** create empty space
   * @param composite composite widget
   * @return control space control
   */
  public static Control newSpacer(Composite composite)
  {
    Label label = new Label(composite,SWT.NONE);

    return (Control)label;
  }

  //-----------------------------------------------------------------------

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param style label style
   * @param accelerator accelerator key code or SWT.NONE
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style, int accelerator)
  {
    Label label;

    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.toLowerCase().indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      else
      {
        text = text+" ["+buttonAcceleratorToText(accelerator)+"]";
      }
    }
    label = new Label(composite,style);
    label.setText(text);

    // set scrolled composite content
    if (composite instanceof ScrolledComposite)
    {
      ((ScrolledComposite)composite).setContent(label);
    }

    return label;
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param style label style
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style)
  {
    return newLabel(composite,text,style,0);
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @return new label
   */
  public static Label newLabel(Composite composite, String text)
  {
    return newLabel(composite,text,SWT.LEFT);
  }

  /** create new label
   * @param composite composite widget
   * @return new label
   */
  public static Label newLabel(Composite composite)
  {
    return newLabel(composite,"");
  }

  //-----------------------------------------------------------------------

  /** create new image
   * @param composite composite widget
   * @param image image
   * @param style label style
   * @return new image
   */
  public static Control newImage(Composite composite, Image image, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setImage(image);

    return (Control)label;
  }

  /** create new image
   * @param composite composite widget
   * @param image image
   * @return new image
   */
  public static Control newImage(Composite composite, Image image)
  {
    return newImage(composite,image,SWT.LEFT);
  }

  //-----------------------------------------------------------------------

  /** create new view
   * @param composite composite widget
   * @param text view text
   * @param style view style
   * @return new view
   */
  public static Label newView(Composite composite, String text, int style)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER|style);
    label.setText(text);

    return label;
  }

  /** create new view
   * @param composite composite widget
   * @param text view text
   * @return new view
   */
  public static Label newView(Composite composite, String text)
  {
    return newView(composite,text,SWT.NONE);
  }

  /** create new view
   * @param composite composite widget
   * @return new view
   */
  public static Label newView(Composite composite)
  {
    return newView(composite,"");
  }

  //-----------------------------------------------------------------------

  /** create new number view
   * @param composite composite widget
   * @param style view style
   * @return new view
   */
  static Label newNumberView(Composite composite, int style)
  {
    Label label;

    label = new Label(composite,style|SWT.BORDER);
    label.setText("0");

    return label;
  }

  /** create new number view
   * @param composite composite widget
   * @return new view
   */
  public static Label newNumberView(Composite composite)
  {
    return newNumberView(composite,SWT.RIGHT);
  }

  //-----------------------------------------------------------------------

  /** create new string view
   * @param composite composite widget
   * @param style view style
   * @return new view
   */
  public static Text newStringView(Composite composite, int style)
  {
    Text text;

    text = new Text(composite,style|SWT.READ_ONLY);
    text.setBackground(composite.getBackground());
    text.setText("");

    return text;
  }

  /** create new string view
   * @param composite composite widget
   * @return new view
   */
  public static Text newStringView(Composite composite)
  {
    return newStringView(composite,SWT.LEFT|SWT.BORDER);
  }

  //-----------------------------------------------------------------------

  /** create new text view
   * @param composite composite widget
   * @param style view style
   * @return new text view
   */
  public static StyledText newTextView(Composite composite, int style)
  {
    StyledText styledText;

    styledText = new StyledText(composite,style|SWT.READ_ONLY);
    styledText.setBackground(composite.getBackground());
    styledText.setText("");

    return styledText;
  }

  /** create new string view
   * @param composite composite widget
   * @return new view
   */
  public static StyledText newTextView(Composite composite)
  {
    return newTextView(composite,SWT.LEFT|SWT.BORDER|SWT.MULTI|SWT.H_SCROLL|SWT.V_SCROLL);
  }

  //-----------------------------------------------------------------------

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param accelerator accelerator key code or SWT.NONE
   * @return new button
   */
  public static Button newButton(Composite composite, String text, int accelerator)
  {
    Button button;

    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.toLowerCase().indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      else
      {
        text = text+" ["+buttonAcceleratorToText(accelerator)+"]";
      }
    }
    button = new Button(composite,SWT.PUSH);
    button.setText(text);

    return button;
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @return new button
   */
  public static Button newButton(Composite composite, String text)
  {
    return newButton(composite,text,0);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @return new button
   */
  public static Button newButton(Composite composite, Image image)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setImage(image);

    return button;
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param text text
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, String text)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setImage(image);
    button.setText(text);

    return button;
  }

  /** create new button
   * @param composite composite widget
   * @return new button
   */
  public static Button newButton(Composite composite)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);

    return button;
  }

  //-----------------------------------------------------------------------

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, final Object data, final String field, boolean value, int accelerator)
  {
    Button button;

    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.toLowerCase().indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      else
      {
        text = text+" ["+buttonAcceleratorToText(accelerator)+"]";
      }
    }
    button = new Button(composite,SWT.CHECK);
    if (text != null) button.setText(text);
    button.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        setField(data,field,widget.getSelection());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    button.setSelection(value);

    return button;
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, final Object data, final String field, boolean value)
  {
    return newCheckbox(composite,text,data,field,value,SWT.NONE);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, final Object data, final String field, int accelerator)
  {
    return newCheckbox(composite,text,data,field,false,accelerator);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, final Object data, final String field)
  {
    return newCheckbox(composite,text,data,field,SWT.NONE);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, int accelerator)
  {
    return newCheckbox(composite,text,null,null,accelerator);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text)
  {
    return newCheckbox(composite,text,SWT.NONE);
  }

  /** create new checkbox
   * @param composite composite widget
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite)
  {
    return newCheckbox(composite,(String)null);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param imageOn,imageOf on/off image
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, final Image imageOn, final Image imageOff, final Object data, final String field, boolean value)
  {
    Button button;

    button = new Button(composite,SWT.TOGGLE);
    button.setImage(imageOff);
    button.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;

        if (widget.getImage() == imageOff)
        {
          // switch on
          widget.setImage(imageOn);
          widget.setSelection(true);
          setField(data,field,true);
        }
        else
        {
          // switch off
          widget.setImage(imageOff);
          widget.setSelection(false);
          setField(data,field,false);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    button.setSelection(value);

    return button;
  }

  /** create new checkbox
   * @param composite composite widget
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, Image imageOff, Image imageOn, final Object data, final String field)
  {
    return newCheckbox(composite,imageOff,imageOn,data,field,false);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param imageOff,imageOn off/on image
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, Image imageOff, Image imageOn)
  {
    return newCheckbox(composite,imageOff,imageOn,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new button
   */
  public static Button newRadio(Composite composite, String text, final Object data, final String field, final Object value)
  {
    Button button;

    button = new Button(composite,SWT.RADIO);
    button.setText(text);
    button.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        setField(data,field,value);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    button.setSelection((getField(data,field) == value));

    return button;
  }

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @return new button
   */
  public static Button newRadio(Composite composite, String text)
  {
    return newRadio(composite,text,null,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @param style text style
   * @return new text widget
   */
  public static Text newText(Composite composite, final Object data, final String field, String value, int style)
  {
    Text text;

    text = new Text(composite,style);
    if      (value != null)
    {
      text.setText(value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      text.setText((String)getField(data,field));
    }

    text.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;
        setField(data,field,widget.getText());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    text.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text widget = (Text)modifyEvent.widget;
        setField(data,field,widget.getText());
      }
    });
    text.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
      }
      public void focusLost(FocusEvent focusEvent)
      {
        Text widget = (Text)focusEvent.widget;
        setField(data,field,widget.getText());
      }
    });

    return text;
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @return new text widget
   */
  public static Text newText(Composite composite, final Object data, final String field, String value)
  {
    return newText(composite,data,field,value,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @return new text widget
   */
  public static Text newText(Composite composite, final Object data, final String field, int style)
  {
    return newText(composite,data,field,"",style);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new text widget
   */
  public static Text newText(Composite composite, final Object data, final String field)
  {
    return newText(composite,data,field,"");
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @return new text widget
   */
  public static Text newText(Composite composite, int style)
  {
    return newText(composite,null,null,style);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  public static Text newText(Composite composite)
  {
    return newText(composite,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new styled text input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @param style text style
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, final Object data, final String field, String value, int style)
  {
    StyledText styledText;

    styledText = new StyledText(composite,style);
    if      (value != null)
    {
      styledText.setText(value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      styledText.setText((String)getField(data,field));
    }

    styledText.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        StyledText widget = (StyledText)selectionEvent.widget;
        setField(data,field,widget.getText());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    styledText.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        StyledText widget = (StyledText)modifyEvent.widget;
        setField(data,field,widget.getText());
      }
    });
    styledText.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
      }
      public void focusLost(FocusEvent focusEvent)
      {
        StyledText widget = (StyledText)focusEvent.widget;
        setField(data,field,widget.getText());
      }
    });

    return styledText;
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, final Object data, final String field, String value)
  {
    return newStyledText(composite,data,field,value,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, final Object data, final String field, int style)
  {
    return newStyledText(composite,data,field,"",style);
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, final Object data, final String field)
  {
    return newStyledText(composite,data,field,"");
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, int style)
  {
    return newStyledText(composite,null,null,style);
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite)
  {
    return newStyledText(composite,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new password input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  public static Text newPassword(Composite composite)
  {
    Text text;

    text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);

    return text;
  }

  //-----------------------------------------------------------------------

  /** create new list widget
   * @param composite composite widget
   * @param style style
   * @return new list widget
   */
  public static List newList(Composite composite, int style)
  {
    List list;

    list = new List(composite,style);

    return list;
  }

  /** create new list widget
   * @param composite composite widget
   * @return new list widget
   */
  public static List newList(Composite composite)
  {
    return newList(composite,SWT.BORDER|SWT.MULTI|SWT.V_SCROLL);
  }

  /** sort list
   * @param list list
   * @param comparator list data comparator
   */
  public static void sortList(List list, Comparator comparator)
  {
    if (!list.isDisposed())
    {
      String[] texts = list.getItems();
      Arrays.sort(texts,comparator);
      list.setItems(texts);
    }
  }

  /** sort list
   * @param list list
   */
  public static void sortList(List list)
  {
    sortList(list,String.CASE_INSENSITIVE_ORDER);
  }

  //-----------------------------------------------------------------------

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field, String value, int style)
  {
    Combo combo;

    combo = new Combo(composite,style);
    if      (value != null)
    {
      combo.setText(value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      combo.setText((String)getField(data,field));
    }

    combo.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo widget = (Combo)selectionEvent.widget;
        setField(data,field,widget.getSelection());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return combo;
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field, String value)
  {
    return newCombo(composite,data,field,value,SWT.BORDER);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field, int style)
  {
    return newCombo(composite,data,field,null,style);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field)
  {
    return newCombo(composite,data,field,SWT.BORDER);
  }

  /** new combo widget
   * @param composite composite widget
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, int style)
  {
    return newCombo(composite,null,null,style);
  }

  /** new combo widget
   * @param composite composite widget
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite)
  {
    return newCombo(composite,SWT.BORDER);
  }

  //-----------------------------------------------------------------------

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store select value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new select widget
   */
  public static Combo newSelect(Composite composite, final Object data, final String field, String value)
  {
    Combo combo;

    combo = new Combo(composite,SWT.BORDER|SWT.READ_ONLY);
    if      (value != null)
    {
      combo.setText(value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      combo.setText((String)getField(data,field));
    }

    combo.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo widget = (Combo)selectionEvent.widget;
        setField(data,field,widget.getText());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return combo;
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store select value or null
   * @param field field name in data structure to set on selection
   * @return new select widget
   */
  public static Combo newSelect(Composite composite, final Object data, final String field)
  {
    return newSelect(composite,data,field,null);
  }

  /** new combo widget
   * @param composite composite widget
   * @return new select widget
   */
  public static Combo newSelect(Composite composite)
  {
    return newSelect(composite,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new option menu
   * @param composite composite widget
   * @return new combo widget
   */
  public static Combo newOptionMenu(Composite composite)
  {
    Combo combo;

    combo = new Combo(composite,SWT.RIGHT|SWT.READ_ONLY);

    return combo;
  }

  //-----------------------------------------------------------------------

  /** create new spinner widget
   * @param composite composite widget
   * @param style style
   * @param min,max min./max. value
   * @return new spinner widget
   */
  public static Spinner newSpinner(Composite composite, int style, int min, int max)
  {
    Spinner spinner;

    spinner = new Spinner(composite,style);
    spinner.setMinimum(min);
    spinner.setMaximum(max);

    return spinner;
  }

  /** create new spinner widget
   * @param composite composite widget
   * @param min,max min./max. value
   * @return new spinner widget
   */
  public static Spinner newSpinner(Composite composite, int min, int max)
  {
    return newSpinner(composite,SWT.BORDER|SWT.RIGHT,min,max);
  }

  /** create new spinner widget
   * @param composite composite widget
   * @param min min. value
   * @return new spinner widget
   */
  public static Spinner newSpinner(Composite composite, int min)
  {
    return newSpinner(composite,min,Integer.MAX_VALUE);
  }

  /** create new spinner widget
   * @param composite composite widget
   * @return new spinner widget
   */
  public static Spinner newSpinner(Composite composite)
  {
    return newSpinner(composite,Integer.MIN_VALUE);
  }

  //-----------------------------------------------------------------------

  /** create new table widget
   * @param composite composite widget
   * @param style style
   * @param object object data
   * @return new table widget
   */
  public static Table newTable(Composite composite, int style)
  {
    Table table;

    table = new Table(composite,style|SWT.BORDER|SWT.MULTI|SWT.FULL_SELECTION);
    table.setLinesVisible(true);
    table.setHeaderVisible(true);

    return table;
  }

  /** create new table widget
   * @param composite composite widget
   * @return new table widget
   */
  public static Table newTable(Composite composite)
  {
    return newTable(composite,SWT.NONE);
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param title column title
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, String title, int style, int width, boolean resizable)
  {
    TableColumn tableColumn = new TableColumn(table,style);
    tableColumn.setText(title);
    tableColumn.setData(new TableLayoutData(0,0,0,0,0,0,0,width,0,resizable ? SWT.DEFAULT : width,resizable ? SWT.DEFAULT : width));
    tableColumn.setWidth(width);
    tableColumn.setResizable(resizable);
    if (width <= 0) tableColumn.pack();

    return tableColumn;
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param title column title
   * @param style style
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, String title, int style, boolean resizable)
  {
    return addTableColumn(table,columnNb,title,style,SWT.DEFAULT,resizable);
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param title column title
   * @param style style
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, String title, int style)
  {
    return addTableColumn(table,columnNb,title,style,true);
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, int style, int width, boolean resizable)
  {
    return addTableColumn(table,columnNb,"",style,width,resizable);
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param style style
   * @param width width of column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, int style, int width)
  {
    return addTableColumn(table,columnNb,style,width,true);
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param style style
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, int style)
  {
    return addTableColumn(table,columnNb,style,SWT.DEFAULT);
  }

  /** show table column
   * @param tableColumn table column to show
   * @param width table column width
   * @param showFlag true to show colume, false for hide
   */
  public static void showTableColumn(TableColumn tableColumn, boolean showFlag)
  {
    if (showFlag)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)tableColumn.getData();

      tableColumn.setWidth(tableLayoutData.minWidth);
      tableColumn.setResizable((tableLayoutData.minWidth != SWT.DEFAULT) || (tableLayoutData.maxWidth != SWT.DEFAULT));
    }
    else
    {
      tableColumn.setWidth(0);
      tableColumn.setResizable(false);
    }
  }

  /** show table column
   * @param tableColumn table column to show
   * @param width table column width
   */
  public static void showTableColumn(TableColumn tableColumn)
  {
    showTableColumn(tableColumn,true);
  }

  /** hide table column
   * @param tableColumn table column to hide
   */
  public static void hideTableColumn(TableColumn tableColumn)
  {
    showTableColumn(tableColumn,false);
  }

  /** get width of table columns
   * @param table table
   * @return table columns width array
   */
  public static int[] getTableColumnWidth(Table table)
  {
    TableColumn[] tableColumns = table.getColumns();
    int[] width = new int[tableColumns.length];
    for (int z = 0; z < tableColumns.length; z++)
    {
      width[z] = tableColumns[z].getWidth();
    }

    return width;
  }

  /** set width of table columns
   * @param table table
   * @param width column width array
   */
  public static void setTableColumnWidth(Table table, int[] width)
  {
    TableColumn[] tableColumns = table.getColumns();
    for (int z = 0; z < Math.min(tableColumns.length,width.length); z++)
    {
      tableColumns[z].setWidth(width[z]);
    }
  }

  /** default table sort selection listener
   */
  final public static SelectionListener DEFAULT_TABLE_SELECTION_LISTENER_STRING = new SelectionListener()
  {
    public void widgetSelected(SelectionEvent selectionEvent)
    {
      TableColumn tableColumn = (TableColumn)selectionEvent.widget;
      Table       table       = tableColumn.getParent();
      Widgets.sortTableColumn(table,tableColumn,String.CASE_INSENSITIVE_ORDER);
    }
    public void widgetDefaultSelected(SelectionEvent selectionEvent)
    {
    }
  };
  final public static SelectionListener DEFAULT_TABLE_SELECTION_LISTENER_INT = new SelectionListener()
  {
    public void widgetSelected(SelectionEvent selectionEvent)
    {
      TableColumn tableColumn = (TableColumn)selectionEvent.widget;
      Table       table       = tableColumn.getParent();
      Widgets.sortTableColumn(table,tableColumn,new Comparator<Integer>()
      {
        public int compare(Integer i1, Integer i2)
        {
          return i1.compareTo(i2);
        }
      });
    }
    public void widgetDefaultSelected(SelectionEvent selectionEvent)
    {
    }
  };
  final public static SelectionListener DEFAULT_TABLE_SELECTION_LISTENER_LONG = new SelectionListener()
  {
    public void widgetSelected(SelectionEvent selectionEvent)
    {
      TableColumn tableColumn = (TableColumn)selectionEvent.widget;
      Table       table       = tableColumn.getParent();
      Widgets.sortTableColumn(table,tableColumn,new Comparator<Long>()
      {
        public int compare(Long i1, Long i2)
        {
          return i1.compareTo(i2);
        }
      });
    }
    public void widgetDefaultSelected(SelectionEvent selectionEvent)
    {
    }
  };

  /** select sort column and sort table
   * @param table table
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  public static void sortTableColumn(Table table, TableColumn tableColumn, Comparator comparator)
  {
    if (!table.isDisposed())
    {
      // get sorting direction
      int sortDirection = table.getSortDirection();
      if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
      if (table.getSortColumn() == tableColumn)
      {
        switch (sortDirection)
        {
          case SWT.UP:   sortDirection = SWT.DOWN; break;
          case SWT.DOWN: sortDirection = SWT.UP;   break;
        }
      }
      table.setSortColumn(tableColumn);
      table.setSortDirection(sortDirection);

      // sort column
      sortTableColumn(table,comparator);
    }
  }

  /** select sort column and sort table
   * @param table table
   * @param columnNb column index to sort by (0..n-1)
   * @param comparator table data comparator
   */
  public static void sortTableColumn(Table table, int columnNb, Comparator comparator)
  {
    sortTableColumn(table,table.getColumn(columnNb),comparator);
  }

  /** sort table by selected sort column
   * @param table table
   * @param comparator table data comparator
   */
  public static void sortTableColumn(Table table, Comparator comparator)
  {
    if (!table.isDisposed())
    {
      TableItem[] tableItems = table.getItems();

      // get sort column index
      int sortColumnIndex = 0;
      for (TableColumn tableColumn : table.getColumns())
      {
        if (table.getSortColumn() == tableColumn)
        {
          break;
        }
        sortColumnIndex++;
      }

      // get sorting direction
      int sortDirection = table.getSortDirection();
      if (sortDirection == SWT.NONE) sortDirection = SWT.UP;

      // sort column
      for (int i = 1; i < tableItems.length; i++)
      {
        boolean sortedFlag = false;
        for (int j = 0; (j < i) && !sortedFlag; j++)
        {
          switch (sortDirection)
          {
            case SWT.UP:
              if (comparator != String.CASE_INSENSITIVE_ORDER)
                sortedFlag = (comparator.compare(tableItems[i].getData(),tableItems[j].getData()) < 0);
              else
                sortedFlag = (comparator.compare(tableItems[i].getText(sortColumnIndex),tableItems[j].getText(sortColumnIndex)) < 0);
              break;
            case SWT.DOWN:
              if (comparator != String.CASE_INSENSITIVE_ORDER)
                sortedFlag = (comparator.compare(tableItems[i].getData(),tableItems[j].getData()) > 0);
              else
                sortedFlag = (comparator.compare(tableItems[i].getText(sortColumnIndex),tableItems[j].getText(sortColumnIndex)) > 0);
              break;
          }
          if (sortedFlag)
          {
            // save data
            Object   data = tableItems[i].getData();
            int columnCount = table.getColumnCount();
            String[] texts;
            if (columnCount > 0)
            {
              texts = new String[table.getColumnCount()];
              for (int z = 0; z < table.getColumnCount(); z++)
              {
                texts[z] = tableItems[i].getText(z);
              }
            }
            else
            {
              texts = new String[1];
              texts[0] = tableItems[i].getText();
            }
            Color foregroundColor = tableItems[i].getForeground();
            Color backgroundColor = tableItems[i].getBackground();
            boolean checked = tableItems[i].getChecked();

            // discard item
            tableItems[i].dispose();

            // create new item
            TableItem tableItem = new TableItem(table,SWT.NONE,j);
            tableItem.setData(data);
            if (columnCount > 0)
            {
              tableItem.setText(texts);
            }
            else
            {
              tableItem.setText(texts[0]);
            }
            tableItem.setForeground(foregroundColor);
            tableItem.setBackground(backgroundColor);
            tableItem.setChecked(checked);

            tableItems = table.getItems();
          }
        }
      }
    }
  }

  /** sort table column
   * @param table table
   * @param tableColumn table column
   * @param sortDirection sorting direction
   */
  public static void sortTable(Table table, TableColumn tableColumn, int sortDirection)
  {
    Event event = new Event();

    table.setSortDirection(sortDirection);
    event.widget = tableColumn;
    tableColumn.notifyListeners(SWT.Selection,event);
  }

  /** sort table column
   * @param table table
   * @param columnNb column index (0..n-1)
   * @param sortDirection sorting direction
   */
  public static void sortTable(Table table, int columnNb, int sortDirection)
  {
    sortTable(table,table.getColumn(columnNb),sortDirection);
  }

  /** sort table column (ascending)
   * @param table table
   * @param columnNb column index (0..n-1)
   */
  public static void sortTable(Table table, int columnNb)
  {
    sortTable(table,columnNb,SWT.UP);
  }

  /** sort table (ascending)
   * @param table table
   */
  public static void sortTable(Table table)
  {
    sortTableColumn(table,String.CASE_INSENSITIVE_ORDER);
  }

  /** get insert position in sorted table
   * @param table table
   * @param comparator table data comparator
   * @param data data
   * @return index in table
   */
  public static int getTableItemIndex(Table table, Comparator comparator, Object data)
  {
    int index = 0;

    if (!table.isDisposed())
    {
      TableItem[] tableItems = table.getItems();

      // get sort column index
      int sortColumnIndex = 0;
      for (TableColumn tableColumn : table.getColumns())
      {
        if (table.getSortColumn() == tableColumn)
        {
          break;
        }
        sortColumnIndex++;
      }

      // get sorting direction
      int sortDirection = table.getSortDirection();
      if (sortDirection == SWT.NONE) sortDirection = SWT.UP;

      // find insert index
      boolean foundFlag = false;
      while ((index < tableItems.length) && !foundFlag)
      {
        switch (sortDirection)
        {
          case SWT.UP:
            if (comparator != String.CASE_INSENSITIVE_ORDER)
              foundFlag = (comparator.compare(tableItems[index].getData(),data) > 0);
            else
              foundFlag = (comparator.compare(tableItems[index].getText(sortColumnIndex),data) > 0);
            break;
          case SWT.DOWN:
            if (comparator != String.CASE_INSENSITIVE_ORDER)
              foundFlag = (comparator.compare(tableItems[index].getData(),data) < 0);
            else
              foundFlag = (comparator.compare(tableItems[index].getText(sortColumnIndex),data) < 0);
            break;
        }
        if (!foundFlag) index++;
      }
    }

    return index;
  }

  /** add table entry
   * @param table table
   * @param index insert before this index in table [0..n-1] or -1
   * @param table entry data
   * @param values values list
   * @return insert index
   */
  public static TableItem insertTableEntry(final Table table, final int index, final Object data, final Object... values)
  {
    /** table insert runnable
     */
    class TableRunnable implements Runnable
    {
      TableItem tableItem = null;

      public void run()
      {
        if (!table.isDisposed())
        {
          if (index >= 0)
          {
            tableItem = new TableItem(table,SWT.NONE,index);
          }
          else
          {
            tableItem = new TableItem(table,SWT.NONE);
          }
          tableItem.setData(data);
          for (int i = 0; i < values.length; i++)
          {
            if (values[i] != null)
            {
              if      (values[i] instanceof String)
              {
//Dprintf.dprintf("i=%d values[i]=%s",i,values[i]);
                tableItem.setText(i,(String)values[i]);
              }
              else if (values[i] instanceof Image)
              {
                tableItem.setImage(i,(Image)values[i]);
              }
            }
          }
        }
      }
    }

    TableRunnable tableRunnable = new TableRunnable();
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(tableRunnable);
    }

    return tableRunnable.tableItem;
  }

  /** insert table entry
   * @param table table
   * @param comparator table entry comperator
   * @param table entry data
   * @param values values list
   * @return table item
   */
  public static TableItem insertTableEntry(final Table table, final Comparator comparator, final Object data, final Object... values)
  {
    /** table insert runnable
     */
    class TableRunnable implements Runnable
    {
      TableItem tableItem = null;

      public void run()
      {
        if (!table.isDisposed())
        {
          tableItem = new TableItem(table,
                                    SWT.NONE,
                                    getTableItemIndex(table,comparator,data)
                                   );
          tableItem.setData(data);
          for (int i = 0; i < values.length; i++)
          {
            if (values[i] != null)
            {
              if      (values[i] instanceof String)
              {
                tableItem.setText(i,(String)values[i]);
              }
              else if (values[i] instanceof Image)
              {
                tableItem.setImage(i,(Image)values[i]);
              }
            }
          }
        }
      }
    };

    TableRunnable tableRunnable = new TableRunnable();
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(tableRunnable);
    }

    return tableRunnable.tableItem;
  }

  /** add table entry
   * @param table table
   * @param table entry data
   * @param values values list
   * @return table item
   */
  public static TableItem addTableEntry(Table table, Object data, Object... values)
  {
    return insertTableEntry(table,-1,data,values);
  }

  /** update table entry
   * @param table table
   * @param data entry data
   * @param values values list
   * @param true if updated, false if not found
   */
  public static boolean updateTableEntry(final Table table, final Object data, final Object... values)
  {
    /** table update runnable
     */
    class TableRunnable implements Runnable
    {
      boolean updatedFlag = false;

      public void run()
      {
        if (!table.isDisposed())
        {
          for (TableItem tableItem : table.getItems())
          {
            if (tableItem.getData() == data)
            {
              for (int i = 0; i < values.length; i++)
              {
                if (values[i] != null)
                {
                  if      (values[i] instanceof String)
                  {
                    tableItem.setText(i,(String)values[i]);
                  }
                  else if (values[i] instanceof Image)
                  {
                    tableItem.setImage(i,(Image)values[i]);
                  }
                }
              }
              updatedFlag = true;
              break;
            }
          }
        }
      }
    }

    TableRunnable tableRunnable = new TableRunnable();
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(tableRunnable);
    }

    return tableRunnable.updatedFlag;
  }

  /** set table entry color
   * @param table table
   * @param table entry data
   * @param foregroundColor foregound color
   * @param backgroundColor background color
   */
  public static void setTableEntryColor(final Table table, final Object data, final Color foregroundColor, final Color backgroundColor)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                tableItem.setForeground(foregroundColor);
                tableItem.setBackground(backgroundColor);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** set table entry color
   * @param table table
   * @param table entry data
   * @param backgroundColor background color
   */
  public static void setTableEntryColor(Table table, Object data, Color backgroundColor)
  {
    setTableEntryColor(table,data,null,backgroundColor);
  }

  /** set table entry color
   * @param table table
   * @param table entry data
   * @param columnNb column (0..n-1)
   * @param foregroundColor foregound color
   * @param backgroundColor background color
   */
  public static void setTableEntryColor(final Table table, final Object data, final int columnNb, final Color foregroundColor, final Color backgroundColor)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                tableItem.setForeground(columnNb,foregroundColor);
                tableItem.setBackground(columnNb,backgroundColor);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** set table entry color
   * @param table table
   * @param table entry data
   * @param columnNb column (0..n-1)
   * @param backgroundColor background color
   */
  public static void setTableEntryColor(Table table, Object data, int columnNb, Color backgroundColor)
  {
    setTableEntryColor(table,data,columnNb,null,backgroundColor);
  }

  /** set table entry font
   * @param table table
   * @param table entry data
   * @param font font
   */
  public static void setTableEntryFont(final Table table, final Object data, final Font font)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                tableItem.setFont(font);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** set table entry font
   * @param table table
   * @param table entry data
   * @param fontData font data
   */
  public static void setTableEntryFont(final Table table, final Object data, final FontData fontData)
  {
    setTableEntryFont(table,data,new Font(table.getDisplay(),fontData));
  }

  /** set table entry font
   * @param table table
   * @param table entry data
   * @param columnNb column (0..n-1)
   * @param font font
   */
  public static void setTableEntryFont(final Table table, final Object data, final int columnNb, final Font font)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                tableItem.setFont(columnNb,font);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** set table entry font
   * @param table table
   * @param table entry data
   * @param columnNb column (0..n-1)
   * @param fontData font data
   */
  public static void setTableEntryFont(final Table table, final Object data, final int columnNb, final FontData fontData)
  {
    setTableEntryFont(table,data,columnNb,new Font(table.getDisplay(),fontData));
  }

  /** set table entry checked
   * @param table table
   * @param table entry data
   * @param checked checked flag
   */
  public static void setTableEntryChecked(final Table table, final Object data, final boolean checked)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                tableItem.setChecked(checked);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** remove table entry
   * @param table table
   * @param table entry data
   */
  public static void removeTableEntry(final Table table, final Object data)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            for (TableItem tableItem : table.getItems())
            {
              if (tableItem.getData() == data)
              {
                table.remove(table.indexOf(tableItem));
                break;
              }
            }
          }
        }
      });
    }
  }

  /** remove table entry
   * @param table table
   * @param tableItem table item to remove
   */
  public static void removeTableEntry(final Table table, final TableItem tableItem)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            table.remove(table.indexOf(tableItem));
          }
        }
      });
    }
  }

  /** remove table entries
   * @param table table
   * @param tableItems table items to remove
   */
  public static void removeTableEntries(Table table, TableItem[] tableItems)
  {
    for (TableItem tableItem : tableItems)
    {
      removeTableEntry(table,tableItems);
    }
  }

  /** remove all table entries
   * @param table table
   */
  public static void removeAllTableEntries(final Table table)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            table.removeAll();
          }
        }
      });
    }
  }

  //-----------------------------------------------------------------------

  /** new progress bar widget
   * @param composite composite widget
   * @param min, max min/max value
   * @return new progress bar widget
   */
  public static ProgressBar newProgressBar(Composite composite, double min, double max)
  {
    ProgressBar progressBar;

    progressBar = new ProgressBar(composite);
    progressBar.setMinimum(min);
    progressBar.setMaximum(max);
    progressBar.setSelection(min);

    return progressBar;
  }

  /** new progress bar widget
   * @param composite composite widget
   * @return new progress bar widget
   */
  public static ProgressBar newProgressBar(Composite composite)
  {
    return newProgressBar(composite,0.0,100.0);
  }

  /** set value of progress bar widget
   * @param progressBar progress bar
   * @param value value
   */
  public static void setProgressBar(final ProgressBar progressBar, final double value)
  {
    if (!progressBar.isDisposed())
    {
      progressBar.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!progressBar.isDisposed())
          {
            progressBar.setSelection(value);
          }
        }
      });
    }
  }

  //-----------------------------------------------------------------------

  /** new tree widget
   * @param composite composite widget
   * @param style style
   * @return new tree widget
   */
  public static Tree newTree(Composite composite, int style)
  {
    Tree tree = new Tree(composite,style|SWT.BORDER|SWT.H_SCROLL|SWT.V_SCROLL);
    tree.setHeaderVisible(true);

    return tree;
  }

  /** new tree widget
   * @param composite composite widget
   * @return new tree widget
   */
  public static Tree newTree(Composite composite)
  {
    return newTree(composite,SWT.NONE);
  }


  /** add column to tree widget
   * @param tree tree widget
   * @param title column title
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new tree column
   */
  public static TreeColumn addTreeColumn(Tree tree, String title, int style, int width, boolean resizable)
  {
    TreeColumn treeColumn = new TreeColumn(tree,style);
    treeColumn.setText(title);
    treeColumn.setWidth(width);
    treeColumn.setResizable(resizable);
    if (width <= 0) treeColumn.pack();

    return treeColumn;
  }

  /** add tree item
   * @param tree tree widget
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(Tree tree, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem = new TreeItem(tree,SWT.CHECK,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  /** add tree item at end
   * @param tree tree widget
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(Tree tree, Object data, boolean folderFlag)
  {
    return addTreeItem(tree,0,data,folderFlag);
  }

  /** add sub-tree item
   * @param parentTreeItem parent tree item
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(TreeItem parentTreeItem, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  /** add sub-tree item ad end
   * @param parentTreeItem parent tree item
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, boolean folderFlag)
  {
    return addTreeItem(parentTreeItem,0,data,folderFlag);
  }

/*
static int rr = 0;
static String indent(int n)
{
  String s="";

  while (n>0) { s=s+"  "; n--; }
  return s;
}
private static void printSubTree(int n, TreeItem parentTreeItem)
{
  System.out.println(indent(n)+parentTreeItem+" ("+parentTreeItem.hashCode()+") count="+parentTreeItem.getItemCount()+" expanded="+parentTreeItem.getExpanded());
  for (TreeItem treeItem : parentTreeItem.getItems())
  {
    printSubTree(n+1,treeItem);
  }
}
private static void printTree(Tree tree)
{
  for (TreeItem treeItem : tree.getItems())
  {
    printSubTree(0,treeItem);
  }
}
*/

  /** re-created tree item (required when sorting by column)
   * @param tree tree
   * @param parentTreeItem parent tree item
   * @param treeItem tree item to re-create
   * @param index index (0..n)
   */
  private static TreeItem recreateSubTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem, int index)
  {
    TreeItem newTreeItem = null;

    if (!tree.isDisposed())
    {
      // save data
      Object   data = treeItem.getData();
      String[] texts = new String[tree.getColumnCount()];
      for (int z = 0; z < tree.getColumnCount(); z++)
      {
        texts[z] = treeItem.getText(z);
      }
      Color foregroundColor = treeItem.getForeground();
      Color backgroundColor = treeItem.getBackground();
      boolean checked = treeItem.getChecked();
      Image image = treeItem.getImage();

      // recreate item
      if (parentTreeItem != null) newTreeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
      else                        newTreeItem = new TreeItem(tree,SWT.NONE,index);
      newTreeItem.setData(data);
      newTreeItem.setText(texts);
      newTreeItem.setForeground(foregroundColor);
      newTreeItem.setBackground(backgroundColor);
      newTreeItem.setChecked(checked);
      newTreeItem.setImage(image);
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        recreateSubTreeItem(tree,newTreeItem,subTreeItem);
      }

      // discard old item
      treeItem.dispose();
    }

    return newTreeItem;
  }

  /** re-created tree item (required when sorting by column)
   * @param tree tree
   * @param parentTreeItem parent tree item
   * @param treeItem tree item to re-create
   */
  private static TreeItem recreateSubTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem)
  {
    return recreateSubTreeItem(tree,parentTreeItem,treeItem,parentTreeItem.getItemCount());
  }

  /** re-created tree item (required when sorting by column)
   * @param tree tree
   * @param parentTreeItem parent tree item
   * @param treeItem tree item to re-create
   * @param index index (0..n)
   */
  private static TreeItem recreateTreeItem(Tree tree, TreeItem treeItem, int index)
  {
    TreeItem newTreeItem = null;

    if (!tree.isDisposed())
    {
      // save data
      Object   data = treeItem.getData();
      String[] texts = new String[tree.getColumnCount()];
      for (int z = 0; z < tree.getColumnCount(); z++)
      {
        texts[z] = treeItem.getText(z);
      }
      boolean checked = treeItem.getChecked();
      Image image = treeItem.getImage();

      // recreate item
      newTreeItem = new TreeItem(tree,SWT.NONE,index);
      newTreeItem.setData(data);
      newTreeItem.setText(texts);
      newTreeItem.setChecked(checked);
      newTreeItem.setImage(image);
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        recreateSubTreeItem(tree,newTreeItem,subTreeItem);
      }

      // discard old item
      treeItem.dispose();
    }

    return newTreeItem;
  }

  /** sort tree column
   * @param tree tree
   * @param treeItem tree item
   * @param subTreeItems sub-tree items to sort
   * @param sortDirection sort directory (SWT.UP, SWT.DOWN)
   * @param comparator comperator to compare two tree items
   */
  private static void sortSubTreeColumn(Tree tree, TreeItem treeItem, TreeItem[] subTreeItems, int sortDirection, Comparator comparator)
  {
    if (!tree.isDisposed())
    {
//rr++;

//System.err.println(indent(rr)+"A "+treeItem+" "+treeItem.hashCode()+" "+treeItem.getItemCount()+" open="+treeItem.getExpanded());
      for (TreeItem subTreeItem : subTreeItems)
      {
        sortSubTreeColumn(tree,subTreeItem,subTreeItem.getItems(),sortDirection,comparator);
      }
//System.err.println(indent(rr)+"B "+subTreeItem+" ("+subTreeItem.hashCode()+") "+subTreeItem.hashCode()+" "+subTreeItem.getItemCount()+" open="+subTreeItem.getExpanded());

      // sort sub-tree
//boolean xx = treeItem.getExpanded();
      for (int i = 0; i < subTreeItems.length; i++)
      {
        boolean sortedFlag = false;
        for (int j = 0; (j <= i) && !sortedFlag; j++)
        {
          switch (sortDirection)
          {
            case SWT.UP:   sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),treeItem.getItem(j).getData()) < 0); break;
            case SWT.DOWN: sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),treeItem.getItem(j).getData()) > 0); break;
          }
          if (sortedFlag)
          {
            recreateSubTreeItem(tree,treeItem,subTreeItems[i],j);
          }
        }
      }
//treeItem.setExpanded(xx);

//rr--;
    }
  }

  /** sort tree column
   * @param tree tree
   * @param subTreeItems sub-tree items to sort
   * @param sortDirection sort directory (SWT.UP, SWT.DOWN)
   * @param comparator comperator to compare two tree items
   */
  private static void sortTreeColumn(Tree tree, TreeItem[] subTreeItems, int sortDirection, Comparator comparator)
  {
    if (!tree.isDisposed())
    {
//rr++;

//System.err.println(indent(rr)+"A "+treeItem+" "+treeItem.hashCode()+" "+treeItem.getItemCount()+" open="+treeItem.getExpanded());
      // sort sub-trees
      for (TreeItem subTreeItem : subTreeItems)
      {
        sortSubTreeColumn(tree,subTreeItem,subTreeItem.getItems(),sortDirection,comparator);
      }
//System.err.println(indent(rr)+"B "+subTreeItem+" ("+subTreeItem.hashCode()+") "+subTreeItem.hashCode()+" "+subTreeItem.getItemCount()+" open="+subTreeItem.getExpanded());

      // sort tree
//boolean xx = treeItem.getExpanded();
      for (int i = 0; i < subTreeItems.length; i++)
      {
        boolean sortedFlag = false;
        for (int j = 0; (j <= i) && !sortedFlag; j++)
        {
          switch (sortDirection)
          {
            case SWT.UP:   sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),tree.getItem(j).getData()) < 0); break;
            case SWT.DOWN: sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),tree.getItem(j).getData()) > 0); break;
          }
          if (sortedFlag)
          {
            recreateTreeItem(tree,subTreeItems[i],j);
          }
        }
      }
//treeItem.setExpanded(xx);

//rr--;
    }
  }

  /** get expanded (open) data entries in tree
   * @param expandedDataSet hash-set for expanded data entries
   * @param treeItem tree item to start
   */
  private static void getExpandedTreeData(HashSet expandedDataSet, TreeItem treeItem)
  {
    if (!treeItem.isDisposed())
    {
      if (treeItem.getExpanded()) expandedDataSet.add(treeItem.getData());
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        getExpandedTreeData(expandedDataSet,subTreeItem);
      }
    }
  }

  /** get expanded (open) tree items in tree
   * @param treeItemSet hash-set for expanded tree items
   * @param treeItem tree item to start
   * @param rootItemsOnly true to collect expanded sub-tree root items only
   */
  private static void getTreeItems(HashSet<TreeItem> treeItemSet, TreeItem treeItem, boolean rootItemsOnly)
  {
    if (!rootItemsOnly || treeItem.getExpanded()) treeItemSet.add(treeItem);
    for (TreeItem subTreeItem : treeItem.getItems())
    {
      getTreeItems(treeItemSet,subTreeItem,rootItemsOnly);
    }
  }

  /** get tree items in tree
   * @param tree tree
   * @param rootItemsOnly true to collect expanded sub-tree root items only
   * @return tree items array
   */
  public static TreeItem[] getTreeItems(Tree tree, boolean rootItemsOnly)
  {
    HashSet<TreeItem> treeItemSet = new HashSet<TreeItem>();
    if (!tree.isDisposed())
    {
      for (TreeItem treeItem : tree.getItems())
      {
        getTreeItems(treeItemSet,treeItem,rootItemsOnly);
      }
    }

    return treeItemSet.toArray(new TreeItem[treeItemSet.size()]);
  }

  /** get tree items in tree
   * @param tree tree
   * @return tree items array
   */
  public static TreeItem[] getTreeItems(Tree tree)
  {
    return getTreeItems(tree,false);
  }

  /** re-expand entries
   * @param expandedEntrySet data entries to re-expand
   * @return treeItem tree item to start
   */
  private static void reExpandTreeItems(HashSet expandedEntrySet, TreeItem treeItem)
  {
    if (!treeItem.isDisposed())
    {
      treeItem.setExpanded(expandedEntrySet.contains(treeItem.getData()));
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        reExpandTreeItems(expandedEntrySet,subTreeItem);
      }
    }
  }

  /** sort tree column
   * @param tree tree
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  public static void sortTreeColumn(Tree tree, TreeColumn treeColumn, Comparator comparator)
  {
    if (!tree.isDisposed())
    {
      TreeItem[] treeItems = tree.getItems();

      // get sorting direction
      int sortDirection = tree.getSortDirection();
      if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
      if (tree.getSortColumn() == treeColumn)
      {
        switch (sortDirection)
        {
          case SWT.UP:   sortDirection = SWT.DOWN; break;
          case SWT.DOWN: sortDirection = SWT.UP;   break;
        }
      }

      // save expanded sub-trees.
      // Note: sub-tree cannot be expanded when either no children exist or the
      // parent is not expanded. Because for sort the tree entries are copied
      // (recreated) the state of the expanded sub-trees are stored here and will
      // later be restored when the complete new tree is created.
      HashSet expandedEntrySet = new HashSet();
      for (TreeItem treeItem : tree.getItems())
      {
        getExpandedTreeData(expandedEntrySet,treeItem);
      }

      // sort column
//printTree(tree);
      sortTreeColumn(tree,tree.getItems(),sortDirection,comparator);

      // restore expanded sub-trees
      for (TreeItem treeItem : tree.getItems())
      {
        reExpandTreeItems(expandedEntrySet,treeItem);
      }

      // set column sort indicators
      tree.setSortColumn(treeColumn);
      tree.setSortDirection(sortDirection);
//System.err.println("2 ---------------");
//printTree(tree);
    }
  }

    /** get width of tree columns
   * @param tree tree
   * @return tree columns width array
   */
  public static int[] getTreeColumnWidth(Tree tree)
  {
    TreeColumn[] treeColumns = tree.getColumns();
    int[] width = new int[treeColumns.length];
    for (int z = 0; z < treeColumns.length; z++)
    {
      width[z] = treeColumns[z].getWidth();
    }

    return width;
  }

  /** set width of tree columns
   * @param tree tree
   * @param width column width array
   */
  public static void setTreeColumnWidth(Tree tree, int[] width)
  {
    TreeColumn[] treeColumns = tree.getColumns();
    for (int z = 0; z < Math.min(treeColumns.length,width.length); z++)
    {
      treeColumns[z].setWidth(width[z]);
    }
  }

  //-----------------------------------------------------------------------

  /** create new sash widget (pane)
   * @param composite composite widget
   * @return new sash widget
   */
  public static Sash newSash(Composite composite, int style)
  {
    Sash sash = new Sash(composite,style);

    return sash;
  }

  /** create new sash form widget
   * @param composite composite widget
   * @return new sash form widget
   */
  public static SashForm newSashForm(Composite composite, int style)
  {
    SashForm sashForm = new SashForm(composite,style);

    return sashForm;
  }

  //-----------------------------------------------------------------------

  /** create new pane widget
   * @param composite composite widget
   * @param style style
   * @param prevPane previous pane
   * @return new pane widget
   */
  public static Pane newPane(Composite composite, int style, Pane prevPane)
  {
    Pane pane = new Pane(composite,style,prevPane);

    return pane;
  }

  /** create new pane widget
   * @param composite composite widget
   * @param style style
   * @return new pane widget
   */
  public static Pane newPane(Composite composite, int style)
  {
    Pane pane = new Pane(composite,style,null);

    return pane;
  }

  //-----------------------------------------------------------------------

  /** create new tab folder
   * @param compositet composite
   * @return new tab folder widget
   */
  public static TabFolder newTabFolder(Composite composite)
  {
    TabFolder tabFolder = new TabFolder(composite,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));

    return tabFolder;
  }

  /** insert tab widget
   * @param tabFolder tab folder
   * @param leftComposite left tab item composite or null
   * @param title title of tab
   * @param data data element
   * @return new composite widget
   */
  public static Composite insertTab(TabFolder tabFolder, Composite leftComposite, String title, Object data)
  {
    // get tab item index
    int index = 0;
    TabItem[] tabItems = tabFolder.getItems();
    for (index = 0; index < tabItems.length; index++)
    {
      if (tabItems[index].getControl() == leftComposite)
      {
        index++;
        break;
      }
    }

    // create tab
    TabItem tabItem = new TabItem(tabFolder,SWT.NONE,index);
    tabItem.setData(data);
    tabItem.setText(title);

    // create composite
    Composite composite = new Composite(tabFolder,SWT.NONE);
    TableLayout tableLayout = new TableLayout();
    tableLayout.marginTop    = 2;
    tableLayout.marginBottom = 2;
    tableLayout.marginLeft   = 2;
    tableLayout.marginRight  = 2;
    composite.setLayout(tableLayout);

    tabItem.setControl(composite);

    return composite;
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @param data data element
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, Object data)
  {
    return insertTab(tabFolder,null,title,data);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title)
  {
    return addTab(tabFolder,title,null);
  }

  /** set tab widget
   * @param tabItem tab item
   * @param composite tab to set
   * @param title title of tab
   * @param data data element
   */
  public static void setTab(TabItem tabItem, Composite composite, String title, Object data)
  {
    tabItem.setData(data);
    tabItem.setText(title);
    tabItem.setControl(composite);
  }

  /** set tab widget
   * @param tabItem tab item
   * @param composite tab to set
   * @param title title of tab
   */
  public static void setTab(TabItem tabItem, Composite composite, String title)
  {
    setTab(tabItem,composite,title,null);
  }

  /** remove tab widget
   * @param tabFolder tab folder
   * @param composite tab to remove
   */
  public static void removeTab(TabFolder tabFolder, Composite composite)
  {
    TabItem tabItem = getTabItem(tabFolder,composite);
    if (tabItem != null)
    {
      composite.dispose();
      tabItem.dispose();
    }
  }

  /** get tab composite array
   * @param tabFolder tab folder
   * @param title title of tab
   * @return tab composites array
   */
  public static Composite[] getTabList(TabFolder tabFolder)
  {
    TabItem[] tabItems = tabFolder.getItems();
    Composite[] tabList = new Composite[tabItems.length];
    for (int z = 0; z < tabItems.length; z++)
    {
      tabList[z] = (Composite)tabItems[z].getControl();
    }
    return tabList;
  }

  /** get tab item
   * @param tabFolder tab folder
   * @param composite tab to find
   * @param tab item or null if not found
   */
  public static TabItem getTabItem(TabFolder tabFolder, Composite composite)
  {
    for (TabItem tabItem : tabFolder.getItems())
    {
      if (tabItem.getControl() == composite)
      {
        return tabItem;
      }
    }
    return null;
  }

  /** move tab
   * @param tabFolder tab folder
   * @param tabItem tab item
   * @param newIndex new tab index (0..n)
   */
  public static void moveTab(TabFolder tabFolder, TabItem tabItem, int newIndex)
  {
    TabItem[] tabItems = tabFolder.getItems();

    // save data
    int     style       = tabItem.getStyle();
    String  title       = tabItem.getText();
    Object  data        = tabItem.getData();
    Control control     = tabItem.getControl();
    String  toolTipText = tabItem.getToolTipText();
    boolean selected    = false;
    for (TabItem selectedTabItem : tabFolder.getSelection())
    {
      if (selectedTabItem == tabItem)
      {
        selected = true;
        break;
      }
    }

    // remove old tab
    tabItem.dispose();

    // create tab a new position
    tabItem = new TabItem(tabFolder,style,newIndex);

    // restore data
    tabItem.setText(title);
    tabItem.setData(data);
    tabItem.setControl(control);
    tabItem.setToolTipText(toolTipText);
    if (selected) tabFolder.setSelection(tabItem);
  }

  /** move tab
   * @param tabFolder tab folder
   * @param composite tab item
   * @param newIndex new tab index (0..n)
   */
  public static void moveTab(TabFolder tabFolder, Composite composite, int newIndex)
  {
    moveTab(tabFolder,getTabItem(tabFolder,composite),newIndex);
  }

  /** set tab widget title
   * @param tabFolder tab folder
   * @param composite tab item
   * @param title title to set
   */
  public static void setTabTitle(TabFolder tabFolder, Composite composite, String title)
  {
    TabItem tabItem = getTabItem(tabFolder,composite);
    if (tabItem != null)
    {
      tabItem.setText(title);
    }
  }

  /** show tab
   * @param tabFolder tab folder
   * @param composite tab to show
   */
  public static void showTab(TabFolder tabFolder, Composite composite)
  {
    TabItem tabItem = getTabItem(tabFolder,composite);
    if (tabItem != null)
    {
      tabFolder.setSelection(tabItem);
    }
  }

  //-----------------------------------------------------------------------

  /** create new canvas widget
   * @param composite composite
   * @param style style
   * @param width/height size of canvas
   * @return new canvas widget
   */
  public static Canvas newCanvas(Composite composite, int style, int width, int height)
  {
    Canvas canvas = new Canvas(composite,style);
    // canvas is a composite; set default layout
    canvas.setLayout(new TableLayout(0.0,0.0,0));
    canvas.setSize(width,height);

    // set scrolled composite content
    if (composite instanceof ScrolledComposite)
    {
      ((ScrolledComposite)composite).setContent(canvas);
    }

    return canvas;
  }

  /** create new canvas widget
   * @param composite composite
   * @param style style
   * @param size size of canvas
   * @return new canvas widget
   */
  public static Canvas newCanvas(Composite composite, int style, Point size)
  {
    return newCanvas(composite,style,size.x,size.y);
  }

  /** create new canvas widget
   * @param composite composite
   * @param style style
   * @return new canvas widget
   */
  public static Canvas newCanvas(Composite composite, int style)
  {
    return newCanvas(composite,style,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** create new canvas widget
   * @param composite composite
   * @return new canvas widget
   */
  public static Canvas newCanvas(Composite composite)
  {
    return newCanvas(composite,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** new slider widget
   * @param composite composite widget
   * @param style style
   * @return new group widget
   */
  public static Slider newSlider(Composite composite, int style)
  {
    Slider slider = new Slider(composite,style);

    return slider;
  }

  //-----------------------------------------------------------------------

  /** new scale widget
   * @param composite composite widget
   * @param style style
   * @return new group widget
   */
  public static Scale newScale(Composite composite, int style)
  {
    Scale scale = new Scale(composite,style);

    return scale;
  }

  //-----------------------------------------------------------------------

  /** create new menu bar
   * @param shell shell
   * @return new menu bar
   */
  public static Menu newMenuBar(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.BAR);
    shell.setMenuBar(menu);

    return menu;
  }

  /** create new popup bar
   * @param shell shell
   * @return new popup menu
   */
  public static Menu newPopupMenu(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.POP_UP);

    return menu;
  }

  /** create new menu
   * @param menu menu bar
   * @param text menu text
   * @return new menu
   */
  public static Menu addMenu(Menu menu, String text)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.CASCADE);
    menuItem.setText(text);
    Menu subMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
    menuItem.setMenu(subMenu);

    return subMenu;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text, int accelerator)
  {
    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+menuAcceleratorToText(accelerator);
    }
    MenuItem menuItem = new MenuItem(menu,SWT.DROP_DOWN);
    menuItem.setText(text);
    if (accelerator != SWT.NONE) menuItem.setAccelerator(accelerator);

    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text)
  {
    return addMenuItem(menu,text,SWT.NONE);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, final Object data, final String field, final Object value, int accelerator)
  {
    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+menuAcceleratorToText(accelerator);
    }
    MenuItem menuItem = new MenuItem(menu,SWT.CHECK);
    menuItem.setText(text);
    if (data != null)
    {
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          setField(data,field,value);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      menuItem.setSelection((getField(data,field) == value));
    }
    if (accelerator != 0) menuItem.setAccelerator(accelerator);

    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, final Object data, final String field, final Object value)
  {
    return addMenuCheckbox(menu,text,data,field,value,SWT.NONE);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param selected true iff checkbox menu entry is selected
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, boolean selected)
  {
    MenuItem menuItem = addMenuCheckbox(menu,text,null,null,null);
    menuItem.setSelection(selected);
    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text)
  {
    return addMenuCheckbox(menu,text,null,null,null);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text, final Object data, final String field, final Object value, int accelerator)
  {
    if (accelerator != SWT.NONE)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+menuAcceleratorToText(accelerator);
    }
    MenuItem menuItem = new MenuItem(menu,SWT.RADIO);
    menuItem.setText(text);
    if (data != null)
    {
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          setField(data,field,value);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      menuItem.setSelection((getField(data,field) == value));
    }
    if (accelerator != SWT.NONE) menuItem.setAccelerator(accelerator);

    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text, final Object data, final String field, final Object value)
  {
    return addMenuRadio(menu,text,data,field,value,SWT.NONE);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param selected true iff radio menu entry is selected
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text, boolean selected)
  {
    MenuItem menuItem = addMenuRadio(menu,text,null,null,null);
    menuItem.setSelection(selected);
    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text)
  {
    return addMenuRadio(menu,text,null,null,null);
  }

  /** add new menu separator
   * @param menu menu
   * @return new menu item
   */
  public static MenuItem addMenuSeparator(Menu menu)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.SEPARATOR);

    return menuItem;
  }

  //-----------------------------------------------------------------------

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @param margin margin or 0
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style, int margin)
  {
    Composite childComposite;

    childComposite = new Composite(composite,style);
    TableLayout tableLayout = new TableLayout(margin);
    childComposite.setLayout(tableLayout);

    // set scrolled composite content
    if (composite instanceof ScrolledComposite)
    {
      ((ScrolledComposite)composite).setContent(childComposite);
    }

    return childComposite;
  }

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style)
  {
    return newComposite(composite,style,0);
  }

  /** new composite widget
   * @param composite composite widget
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite)
  {
    return newComposite(composite,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @param margin margin or 0
   * @return new composite widget
   */
  public static ScrolledComposite newScrolledComposite(Composite composite, int style, int margin)
  {
    ScrolledComposite childComposite;

    childComposite = new ScrolledComposite(composite,style);
    TableLayout tableLayout = new TableLayout(margin);
    childComposite.setLayout(tableLayout);

    return childComposite;
  }

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @return new composite widget
   */
  public static ScrolledComposite newScrolledComposite(Composite composite, int style)
  {
    return newScrolledComposite(composite,style,0);
  }

  /** new composite widget
   * @param composite composite widget
   * @return new composite widget
   */
  public static ScrolledComposite newScrolledComposite(Composite composite)
  {
    return newScrolledComposite(composite,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** new group widget
   * @param composite composite widget
   * @param title group title
   * @param style style
   * @param margin margin or 0
   * @return new group widget
   */
  public static Group newGroup(Composite composite, String title, int style, int margin)
  {
    Group group;

    group = new Group(composite,style);
    group.setText(title);
    TableLayout tableLayout = new TableLayout(margin);
    group.setLayout(tableLayout);

    return group;
  }

  /** new group widget
   * @param composite composite widget
   * @param title group title
   * @param style style
   * @return new group widget
   */
  public static Group newGroup(Composite composite, String title, int style)
  {
    return newGroup(composite,title,style,0);
  }

  /** new group widget
   * @param composite composite widget
   * @param title group title
   * @return new group widget
   */
  public static Group newGroup(Composite composite, String title)
  {
    return newGroup(composite,title,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** add modify listener
   * @param widgetModifyListener listener to add
   */
  public static void addModifyListener(WidgetModifyListener widgetModifyListener)
  {
    listenersList.add(widgetModifyListener);
    widgetModifyListener.modified();
  }

  /** execute modify listeners
   * @param variable modified variable
   */
  public static void modified(Object object)
  {
    for (WidgetModifyListener widgetModifyListener : listenersList)
    {
      if (widgetModifyListener.equals(object))
      {
        widgetModifyListener.modified();
      }
    }
  }

  /** add event listener
   * @param widgetEventListener listener to add
   */
  static void addEventListener(WidgetEventListener widgetEventListener)
  {
    widgetEventListener.add();
  }

  /** trigger widget event
   * @param widgetEvent widget event to trigger
   */
  static void trigger(WidgetEvent widgetEvent)
  {
    widgetEvent.trigger();
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   * @param index index of event
   * @param item item of event
   */
  public static void notify(Control control, int type, Widget widget, int index, Widget item)
  {
    if (!control.isDisposed() && control.isEnabled())
    {
      Event event = new Event();
      event.widget = widget;
      event.index  = index;
      event.item   = item;
      control.notifyListeners(type,event);
    }
  }

  /** event notification
   * @param control control
   * @param type event type to generate
   * @param index index of event
   * @param widget widget of event
   */
  public static void notify(Control control, int type, int index, Widget widget)
  {
    notify(control,type,widget,index,null);
  }

  /** event notification
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   * @param item item of event
   */
  public static void notify(Control control, int type, Widget widget, Widget item)
  {
    notify(control,type,widget,-1,item);
  }

  /** event notification
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   */
  public static void notify(Control control, int type, Widget widget)
  {
    notify(control,type,widget,-1,null);
  }

  /** event notification
   * @param control control
   * @param type event type to generate
   * @param index index of event
   */
  public static void notify(Control control, int type, int index)
  {
    notify(control,type,control,index,null);
  }

  /** event notification
   * @param control control
   * @param type event type to generate
   */
  public static void notify(Control control, int type)
  {
    notify(control,type,control);
  }

  /** event notification
   * @param control control
   */
  public static void notify(Control control)
  {
    notify(control,SWT.Selection);
  }

  //-----------------------------------------------------------------------

  /** set clipboard with text
   * @param clipboard clipboard
   * @param text text
   */
  public static void setClipboard(Clipboard clipboard, String text)
  {
    clipboard.setContents(new Object[]{text},new Transfer[]{TextTransfer.getInstance()});
  }

  /** set clipboard with text lines
   * @param clipboard clipboard
   * @param texts text lines
   */
  public static void setClipboard(Clipboard clipboard, String lines[])
  {
    StringBuilder buffer = new StringBuilder();
    String lineSeparator = System.getProperty("line.separator");
    for (String line : lines)
    {
      buffer.append(line);
      buffer.append(lineSeparator);
    }
    setClipboard(clipboard,buffer.toString());
  }

  /** set clipboard with text from label
   * @param clipboard clipboard
   * @param label label
   */
  public static void setClipboard(Clipboard clipboard, Label label)
  {
    setClipboard(clipboard,label.getText());
  }

  /** set clipboard with text from text
   * @param clipboard clipboard
   * @param text text
   */
  public static void setClipboard(Clipboard clipboard, Text text)
  {
    setClipboard(clipboard,text.getSelectionText());
  }

  /** set clipboard with text from styled text
   * @param clipboard clipboard
   * @param styledText styled text
   */
  public static void setClipboard(Clipboard clipboard, StyledText styledText)
  {
    setClipboard(clipboard,styledText.getSelectionText());
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param tableItem table item
   */
  public static void setClipboard(Clipboard clipboard, TableItem tableItem)
  {
    Table table       = tableItem.getParent();
    int   columnCount = table.getColumnCount();

    StringBuilder buffer = new StringBuilder();
    for (int i = 0; i < columnCount ; i++)
    {
      if (i > 0) buffer.append('\t');
      buffer.append(tableItem.getText(i));
    }

    setClipboard(clipboard,buffer.toString());
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param tableItem table item
   * @param columnNb or -1 for all columns
   */
  public static void setClipboard(Clipboard clipboard, TableItem[] tableItems, int columnNb)
  {
    if (tableItems.length > 0)
    {
      Table table = tableItems[0].getParent();

      StringBuilder buffer = new StringBuilder();
      String lineSeparator = System.getProperty("line.separator");
      if (columnNb >= 0)
      {
        for (int z = 0; z < tableItems.length ; z++)
        {
          buffer.append(tableItems[z].getText(columnNb)); buffer.append(lineSeparator);
        }
      }
      else
      {
        int columnCount = Math.max(table.getColumnCount(),1);
        for (int z = 0; z < tableItems.length ; z++)
        {
          for (int i = 0; i < columnCount ; i++)
          {
            if (i > 0) buffer.append('\t');
            buffer.append(tableItems[z].getText(i));
          }
          buffer.append(lineSeparator);
        }
      }
      setClipboard(clipboard,buffer.toString());
    }
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param tableItem table item
   */
  public static void setClipboard(Clipboard clipboard, TableItem[] tableItems)
  {
    setClipboard(clipboard,tableItems,-1);
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param treeItems tree item
   * @param columnNb or -1 for all columns
   */
  public static void setClipboard(Clipboard clipboard, TreeItem[] treeItems, int columnNb)
  {
    if (treeItems.length > 0)
    {
      Tree tree = treeItems[0].getParent();

      StringBuilder buffer = new StringBuilder();
      String lineSeparator = System.getProperty("line.separator");
      if (columnNb >= 0)
      {
        for (int z = 0; z < treeItems.length ; z++)
        {
          buffer.append(treeItems[z].getText(columnNb)); buffer.append(lineSeparator);
        }
      }
      else
      {
        int columnCount = Math.max(tree.getColumnCount(),1);
        for (int z = 0; z < treeItems.length ; z++)
        {
          for (int i = 0; i < columnCount ; i++)
          {
            if (i > 0) buffer.append('\t');
            buffer.append(treeItems[z].getText(i));
          }
          buffer.append(lineSeparator);
        }
      }
      setClipboard(clipboard,buffer.toString());
    }
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param treeItems tree item
   */
  public static void setClipboard(Clipboard clipboard, TreeItem[] treeItems)
  {
    setClipboard(clipboard,treeItems,-1);
  }

  /** add copy listener (copy content to clipboard on ctrl-c)
   * @param control control
   * @param clipboard clipboard to copy to
   */
  public static void addCopyListener(final Control control, final Clipboard clipboard)
  {
    control.addKeyListener(new KeyListener()
    {
      public void keyPressed(KeyEvent keyEvent)
      {
        if (((keyEvent.stateMask & SWT.CTRL) != 0) && (keyEvent.keyCode == 'c'))
        {
//Dprintf.dprintf("control=%s",control);
          if      (control instanceof Text)
          {
            setClipboard(clipboard,(Text)control);
          }
          else if (control instanceof StyledText)
          {
            setClipboard(clipboard,(StyledText)control);
          }
          else if (control instanceof Table)
          {
            setClipboard(clipboard,((Table)control).getSelection());
          }
          else if (control instanceof List)
          {
            setClipboard(clipboard,((List)control).getSelection());
          }
        }
      }
      public void keyReleased(KeyEvent keyEvent)
      {
      }
    });
  }

  /** flash widget
   * @param control control
   */
  public static void flash(Control control)
  {
    Display display = control.getDisplay();

    control.setBackground(display.getSystemColor(SWT.COLOR_RED)); display.update();
    display.beep();
    try { Thread.sleep(350); } catch (InterruptedException exception) { /* ignored */ };
    control.setBackground(null); display.update();
  }
}

/* end of file */
