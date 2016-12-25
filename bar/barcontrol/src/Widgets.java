/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: simple widgets functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.File;
import java.io.InputStream;
import java.io.ByteArrayInputStream;

import java.lang.reflect.Array;

import java.net.URL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collections;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Scanner;

// graphics
import org.eclipse.swt.custom.CTabFolder;
import org.eclipse.swt.custom.CTabItem;
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
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.Device;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.FontData;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.DateTime;
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
import org.eclipse.swt.widgets.TypedListener;
import org.eclipse.swt.widgets.Widget;

import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.events.PaintEvent;

/****************************** Classes ********************************/

/** widget variable
 */
class WidgetVariable<T>
{
  private final String   name;
  private final Class    type;
  private final String[] values;     // possible values or null
  private T              value;      // value

  /** get widget variable instance
   * @param object value
   * @param name name
   * @return instance of widget variable
   */
  public static WidgetVariable getInstance(Object object, String name)
  {
    WidgetVariable widgetVariable;

    if      (object instanceof Boolean)
    {
      widgetVariable = new WidgetVariable<Boolean>((Boolean)object,name);
    }
    else if (object instanceof Long)
    {
      widgetVariable = new WidgetVariable<Long>((Long)object,name);
    }
    else if (object instanceof Double)
    {
      widgetVariable = new WidgetVariable<Double>((Double)object,name);
    }
    else if (object instanceof String)
    {
      widgetVariable = new WidgetVariable<String>((String)object,name);
    }
    else if (object instanceof Enum)
    {
      widgetVariable = new WidgetVariable<Enum>((Enum)object,name);
    }
    else
    {
      widgetVariable = new WidgetVariable<Object>(object,name);
    }

    return widgetVariable;
  }

  /** get widget variable instance
   * @param object value
   * @return instance of widget variable
   */
  public static WidgetVariable getInstance(Object object)
  {
    return getInstance(object,(String)null);
  }

  /** create widget variable
   * @param name name
   * @param value value
   * @param values values
   */
  WidgetVariable(String name, T value, Object values)
  {
    this.name   = name;
    this.type   = value.getClass();
    this.values = null;
    this.value  = value;
  }

  /** create widget variable
   * @param name name
   * @param value value
   */
  WidgetVariable(String name, T value)
  {
    this(name,value,(Object)null);
  }

  /** create widget variable
   * @param value value
   * @param values values
   */
  WidgetVariable(T value, Object values)
  {
    this((String)null,value,values);
  }

  /** create widget variable
   * @param value value
   */
  WidgetVariable(T value)
  {
    this(value,(Object)null);
  }

  /** create widget variable
   * @param name name
   * @param value value
   * @param values values
   */
  WidgetVariable(String name, String[] values, T value)
  {
    this.name   = name;
    this.type   = String.class;
    this.values = values;
    this.value  = value;
  }

  /** create widget variable
   * @param name name
   * @param b/i/l/d/string/enumeration value
   */
  WidgetVariable(String name, boolean b)
  {
    this.name   = name;
    this.type   = Boolean.class;
    this.values = null;
    this.value  = (T)new Boolean(b);
  }
  WidgetVariable(boolean b)
  {
    this((String)null,b);
  }
  WidgetVariable(String name, int i)
  {
    this.name   = name;
    this.type   = Integer.class;
    this.values = null;
    this.value  = (T)new Integer(i);
  }
  WidgetVariable(int i)
  {
    this((String)null,i);
  }
  WidgetVariable(String name, long l)
  {
    this.name   = name;
    this.type   = Long.class;
    this.values = null;
    this.value  = (T)new Long(l);
  }
  WidgetVariable(long l)
  {
    this((String)null,l);
  }
  WidgetVariable(String name, double d)
  {
    this.name   = name;
    this.type   = Double.class;
    this.values = null;
    this.value  = (T)new Double(d);
  }
  WidgetVariable(double d)
  {
    this((String)null,d);
  }

  /** get variable name
   * @return name
   */
  String getName()
  {
    return name;
  }

  /** get variable type
   * @return type
   */
  Class getType()
  {
    return type;
  }

  /** get value
   * @return value
   */
  T get()
  {
    return value;
  }

  /** set value
   * @param value value
   * @return true iff changed
   */
  boolean set(T value)
  {
    boolean changedFlag;

    changedFlag = (!this.value.equals(value));

    this.value = value;
    Widgets.modified(this);

    return changedFlag;
  }

  /** get boolean value
   * @return true or false
   */
  boolean getBoolean()
  {
    assert type == Boolean.class;

    return (Boolean)value;
  }

  /** get integer value
   * @return value
   */
  int getInteger()
  {
    assert type == Integer.class;

    return (Integer)value;
  }

  /** get long value
   * @return value
   */
  long getLong()
  {
    assert type == Long.class;

    return (Long)value;
  }

  /** get double value
   * @return value
   */
  double getDouble()
  {
    assert type == Double.class;

    return (Double)value;
  }

  /** get string value
   * @return value
   */
  String getString()
  {
    assert (type == String.class) || (type == Enum.class);

    return (String)value;
  }

  /** set boolean value
   * @param value value
   * @return true iff changed
   */
  boolean set(boolean value)
  {
    boolean changedFlag;

    assert type == Boolean.class;

    changedFlag = ((Boolean)this.value != value);

    this.value = (T)new Boolean(value);
    Widgets.modified(this);

    return changedFlag;
  }

  /** set int value
   * @param l value
   * @return true iff changed
   */
  boolean set(int value)
  {
    boolean changedFlag;

    assert type == Integer.class;

    changedFlag = ((Integer)this.value != value);

    this.value = (T)new Integer(value);
    Widgets.modified(this);

    return changedFlag;
  }

  /** set long value
   * @param l value
   * @return true iff changed
   */
  boolean set(long value)
  {
    boolean changedFlag;

    assert type == Long.class;

    changedFlag = ((Long)this.value != value);

    this.value = (T)new Long(value);
    Widgets.modified(this);

    return changedFlag;
  }

  /** set double value
   * @param value value
   * @return true iff changed
   */
  boolean set(double value)
  {
    boolean changedFlag;

    assert type == Double.class;

    changedFlag = ((Double)this.value != value);

    this.value = (T)new Double(value);
    Widgets.modified(this);

    return changedFlag;
  }

  /** set string value
   * @param value value
   * @return true iff changed
   */
  boolean set(String value)
  {
    boolean changedFlag = false;

    assert (type == String.class) || (type == Enum.class);

    if      (type == String.class)
    {
      changedFlag = ((String)this.value != value);

      this.value = (T)new String(value);
      Widgets.modified(this);
    }
    else if (type == Enum.class)
    {
      for (Object v : values)
      {
        if (((String)v).equals(value))
        {
          changedFlag = ((String)this.value != value);

          this.value = (T)new String(value);
          Widgets.modified(this);
        }
      }
    }

    return changedFlag;
  }

  /** compare string values
   * @param value value to compare with
   * @return true iff equal
   */
  public boolean equals(String value)
  {
    String s;
    if      (type == Boolean.class) s = ((Boolean)this.value).toString();
    else if (type == Long.class   ) s = ((Long   )this.value).toString();
    else if (type == Double.class ) s = ((Double )this.value).toString();
    else if (type == String.class ) s = (String)this.value;
    else if (type == Enum.class   ) s = (String)this.value;
    else                            s = (this.value != null) ? this.value.toString() : "";

    return (s != null) ? s.equals(value) : (value == null);
  }

  /** compare object reference
   * @param value object to compare with
   * @return true iff equal
   */
  public boolean equals(Object value)
  {
    return this.value == value;
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    return "WidgetVariable {"+name+", "+type.toString()+", "+value+"}";
  }
}

/** widget modify listener
 */
class WidgetModifyListener
{
  private Widget           widget;
  private WidgetVariable[] variables;

  // cached text for widget
  private String cachedText = "";

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
  WidgetModifyListener(Widget widget, WidgetVariable... variables)
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
    WidgetVariable widgetVariable;
    if      (object instanceof Boolean)
    {
      widgetVariable = new WidgetVariable<Boolean>((Boolean)object);
    }
    else if (object instanceof Integer)
    {
      widgetVariable = new WidgetVariable<Integer>((Integer)object);
    }
    else if (object instanceof Long)
    {
      widgetVariable = new WidgetVariable<Long>((Long)object);
    }
    else if (object instanceof Double)
    {
      widgetVariable = new WidgetVariable<Double>((Double)object);
    }
    else if (object instanceof String)
    {
      widgetVariable = new WidgetVariable<String>((String)object);
    }
    else if (object instanceof Enum)
    {
      widgetVariable = new WidgetVariable<Enum>((Enum)object);
    }
    else
    {
      widgetVariable = new WidgetVariable<Object>(object);
    }

    this.widget       = widget;
    this.variables    = new WidgetVariable[1];
    this.variables[0] = WidgetVariable.getInstance(object);
  }

  /** create widget listener
   * @param widget widget
   * @param objects objects
   */
  WidgetModifyListener(Widget widget, Object... objects)
  {
    this.widget    = widget;
    this.variables = new WidgetVariable[objects.length];
    for (int i = 0; i < objects.length; i++)
    {
      this.variables[i] = WidgetVariable.getInstance(objects[i]);
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
   * @param otherWidgetVariable variable object
   * @return true iff equal variable object is equal to some variable
   */
  public boolean equals(WidgetVariable otherVariable)
  {
    for (WidgetVariable variable : variables)
    {
      if (variable != null)
      {
        if (variable == otherVariable) return true;
      }
    }

    return false;
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
        if (variable.get() == object) return true;
      }
    }

    return false;
  }

  /** set text or selection for widget according to value of variable
   * @param widget widget widget to set
   * @param variable variable
   */
  void modified(Widget widget, WidgetVariable variable)
  {
    if (!widget.isDisposed())
    {
      if      (widget instanceof Label)
      {
        Label widgetLabel = (Label)widget;

        String text = getString(variable);
        if (text == null)
        {
          if      (variable.getType() == Long.class  ) text = Long.toString(variable.getLong());
          else if (variable.getType() == Double.class) text = Double.toString(variable.getDouble());
          else if (variable.getType() == String.class) text = variable.getString();
        }
        if ((text != null) && !text.equals(cachedText))
        {
          // Fix layout: save current bounds and restore after pack()
          Rectangle bounds = widgetLabel.getBounds();
          widgetLabel.setText(text);
          widgetLabel.pack();
          widgetLabel.setBounds(bounds);

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
            if      (variable.getType() == Long.class  ) text = Long.toString(variable.getLong());
            else if (variable.getType() == Double.class) text = Double.toString(variable.getDouble());
            else if (variable.getType() == String.class) text = variable.getString();
          }
          if ((text != null) && !text.equals(cachedText))
          {
            // Fix layout: save current bounds and restore after pack()
            Rectangle bounds = widgetButton.getBounds();
            widgetButton.setText(text);
            widgetButton.pack();
            widgetButton.setBounds(bounds);

            cachedText = text;
          }
        }
        else if ((widgetButton.getStyle() & SWT.CHECK) == SWT.CHECK)
        {
          boolean selection = false;
          if      (variable.getType() == Boolean.class) selection = variable.getBoolean();
          else if (variable.getType() == Long.class   ) selection = (variable.getLong() != 0);
          else if (variable.getType() == Double.class ) selection = (variable.getDouble() != 0);
          widgetButton.setSelection(selection);
        }
        else if ((widgetButton.getStyle() & SWT.RADIO) == SWT.RADIO)
        {
          boolean selection = false;
          if      (variable.getType() == Boolean.class) selection = variable.getBoolean();
          else if (variable.getType() == Long.class   ) selection = (variable.getLong() != 0);
          else if (variable.getType() == Double.class ) selection = (variable.getDouble() != 0);
          widgetButton.setSelection(selection);
        }
      }
      else if (widget instanceof Combo)
      {
        Combo widgetCombo = (Combo)widget;

        String text = getString(variable);
        if (text == null)
        {
          if      (variable.getType() == Boolean.class) text = Boolean.toString(variable.getBoolean());
          else if (variable.getType() == Long.class   ) text = Long.toString(variable.getLong());
          else if (variable.getType() == Double.class ) text = Double.toString(variable.getDouble());
          else if (variable.getType() == String.class ) text = variable.getString();
          else if (variable.getType() == Enum.class   ) text = variable.getString();
        }
        if ((text != null) && !text.equals(cachedText))
        {
          // Fix layout: save current bounds and restore after pack()
          Rectangle bounds = widgetCombo.getBounds();
          widgetCombo.setText(text);
          widgetCombo.pack();
          widgetCombo.setBounds(bounds);

          cachedText = text;
        }
      }
      else if (widget instanceof Text)
      {
        Text widgetText = (Text)widget;

        String text = getString(variable);
        if (text == null)
        {
          if      (variable.getType() == Long.class  ) text = Long.toString(variable.getLong());
          else if (variable.getType() == Double.class) text = Double.toString(variable.getDouble());
          else if (variable.getType() == String.class) text = variable.getString();
        }
        if ((text != null) && !text.equals(cachedText))
        {
          // Fix layout: save current bounds and restore after pack()
          Rectangle bounds = widgetText.getBounds();
          widgetText.setText(text);
          widgetText.pack();
          widgetText.setBounds(bounds);

          cachedText = text;
        }
      }
      else if (widget instanceof StyledText)
      {
        StyledText widgetStyledText = (StyledText)widget;

        String text = getString(variable);
        if (text == null)
        {
          if      (variable.getType() == Long.class  ) text = Long.toString(variable.getLong());
          else if (variable.getType() == Double.class) text = Double.toString(variable.getDouble());
          else if (variable.getType() == String.class) text = variable.getString();
        }
        if ((text != null) && !text.equals(cachedText))
        {
          // Fix layout: save current bounds and restore after pack()
          Rectangle bounds = widgetStyledText.getBounds();
          widgetStyledText.setText(text);
          widgetStyledText.pack();
          widgetStyledText.setBounds(bounds);

          cachedText = text;
        }
      }
      else if (widget instanceof Spinner)
      {
        Spinner widgetSpinner = (Spinner)widget;

        int value = 0;
        if      (variable.getType() == Long.class  ) value = (int)variable.getLong();
        else if (variable.getType() == Double.class) value = (int)variable.getDouble();
        widgetSpinner.setSelection(value);
      }
      else if (widget instanceof Slider)
      {
        Slider widgetSlider = (Slider)widget;

        int value = 0;
        if      (variable.getType() == Long.class  ) value = (int)variable.getLong();
        else if (variable.getType() == Double.class) value = (int)variable.getDouble();
        widgetSlider.setSelection(value);
      }
      else if (widget instanceof Scale)
      {
        Scale widgetScale = (Scale)widget;

        int value = 0;
        if      (variable.getType() == Long.class  ) value = (int)variable.getLong();
        else if (variable.getType() == Double.class) value = (int)variable.getDouble();
        widgetScale.setSelection(value);
      }
      else if (widget instanceof ProgressBar)
      {
        ProgressBar widgetProgressBar = (ProgressBar)widget;

        double value = 0.0;
        if      (variable.getType() == Long.class  ) value = (double)variable.getLong();
        else if (variable.getType() == Double.class) value = variable.getDouble();
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
   * @param button button to notify about modified variable
   * @param variable variable
   */
  void modified(Button button, WidgetVariable variable)
  {
    modified((Widget)button,variable);
  }

  /** modified handler
   * Note: required because it can be overwritten by specific handler
   * @param combo combo to notify about modified variable
   * @param variable variable
   */
  void modified(Combo combo, WidgetVariable variable)
  {
    modified((Widget)combo,variable);
  }

  /** modified handler
   * Note: required because it can be overwritten by specific handler
   * @param text text to notify about modified variable
   * @param variable variable
   */
  void modified(Text text, WidgetVariable variable)
  {
    modified((Widget)text,variable);
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

  /** set text or selection for widget according to value of variable
   * @param widget widget to notify about modified variable
   * @param variables variables
   */
  void modified(Widget widget, WidgetVariable[] variables)
  {
    if (!widget.isDisposed())
    {
      for (WidgetVariable variable : variables)
      {
        modified(widget,variable);
      }
    }
  }

  /** set text or selection for widget according to value of variable
   * @param control control to notify about modified variable
   * @param variables variables
   */
  void modified(Control control, WidgetVariable[] variables)
  {
    if (!control.isDisposed())
    {
      for (WidgetVariable variable : variables)
      {
        modified(control,variable);
      }
    }
  }

  /** set text or selection for widget according to value of variable
   * @param button button to notify about modified variable
   * @param variables variables
   */
  void modified(Button button, WidgetVariable[] variables)
  {
    modified((Widget)button,variables);
  }

  /** set text or selection for widget according to value of variable
   * @param combo combo to notify about modified variable
   * @param variables variables
   */
  void modified(Combo combo, WidgetVariable[] variables)
  {
    modified((Widget)combo,variables);
  }

  /** set text or selection for widget according to value of variable
   * @param text text to notify about modified variable
   * @param variables variables
   */
  void modified(Text text, WidgetVariable[] variables)
  {
    modified((Widget)text,variables);
  }

  /** set text or selection for widget according to value of variable
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
   * @param widget widget modified
   */
  void modified(Widget widget)
  {
    modified(widget,variables);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param control control modified
   */
  void modified(Control control)
  {
    modified(control,variables);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param label label modified
   */
  void modified(Label label)
  {
    modified((Control)label);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param button button modified
   */
  void modified(Button button)
  {
    modified((Control)button);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param combo combo modified
   */
  void modified(Combo combo)
  {
    modified((Control)combo);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param text text modified
   */
  void modified(Text text)
  {
    modified((Control)text);
  }

  /** notify modify variable
   * Note: required because it can be overwritten by specific handler
   * @param menuItem menu item modified
   */
  void modified(MenuItem menuItem)
  {
    modified(menuItem,variables);
  }

  /** notify modify variable
   */
  public void modified()
  {
    if (!widget.isDisposed())
    {
      if      (widget instanceof Label)
      {
        modified((Label)widget);
      }
      else if (widget instanceof Button)
      {
        modified((Button)widget);
      }
      else if (widget instanceof Combo)
      {
        modified((Combo)widget);
      }
      else if (widget instanceof Text)
      {
        modified((Text)widget);
      }
      else if (widget instanceof MenuItem)
      {
        modified((MenuItem)widget);
      }
      else if (widget instanceof Control)
      {
        modified((Control)widget);
      }
      else
      {
        modified(widget);
      }
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
    WidgetEventListener widgetEventListeners[] = widgetEventListenerSet.toArray(new WidgetEventListener[widgetEventListenerSet.size()]);
    for (WidgetEventListener widgetEventListener : widgetEventListeners)
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
    if (!widget.isDisposed())
    {
      if      (widget instanceof Control)
      {
        trigger((Control)widget);
      }
      else if (widget instanceof MenuItem)
      {
        trigger((MenuItem)widget);
      }
      else
      {
        trigger(widget);
      }
    }
    else
    {
      remove();
    }
  }
}

//????
/** list data entry
 */
class ListItem
{
  String text;
  Object data;

  ListItem(String text, Object data)
  {
    this.text = text;
    this.data = data;
  }
}

/** Widgets class
 */
class Widgets
{
  //-----------------------------------------------------------------------

  // hash of widgets listeners
  private static HashSet<WidgetModifyListener> widgetModifyListenerHashSet = new HashSet<WidgetModifyListener>();

  // images
  private static ImageData IMAGE_CLOSE_DATA;

  static
  {
    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' images/close.png | awk 'BEGIN {n=0;); if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s",$1); n++; }'
    final byte[] IMAGE_CLOSE_DATA_ARRAY =
    {
      (byte)0x89, (byte)0x50, (byte)0x4e, (byte)0x47, (byte)0x0d, (byte)0x0a, (byte)0x1a, (byte)0x0a, (byte)0x00,
      (byte)0x00, (byte)0x00, (byte)0x0d, (byte)0x49, (byte)0x48, (byte)0x44, (byte)0x52, (byte)0x00, (byte)0x00,
      (byte)0x00, (byte)0x10, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x10, (byte)0x08, (byte)0x06, (byte)0x00,
      (byte)0x00, (byte)0x00, (byte)0x1f, (byte)0xf3, (byte)0xff, (byte)0x61, (byte)0x00, (byte)0x00, (byte)0x02,
      (byte)0xa3, (byte)0x49, (byte)0x44, (byte)0x41, (byte)0x54, (byte)0x78, (byte)0xda, (byte)0x85, (byte)0xcd,
      (byte)0x7f, (byte)0x4c, (byte)0xcc, (byte)0x71, (byte)0x1c, (byte)0xc7, (byte)0xf1, (byte)0x67, (byte)0xdf,
      (byte)0x13, (byte)0x92, (byte)0xb8, (byte)0x42, (byte)0xb3, (byte)0xa5, (byte)0xd6, (byte)0xbf, (byte)0x19,
      (byte)0x33, (byte)0x6d, (byte)0xca, (byte)0xfc, (byte)0xe1, (byte)0x47, (byte)0xcb, (byte)0x10, (byte)0xfe,
      (byte)0xb0, (byte)0x65, (byte)0xcb, (byte)0xd6, (byte)0x94, (byte)0x26, (byte)0x63, (byte)0xc3, (byte)0x1f,
      (byte)0xcc, (byte)0xfa, (byte)0x31, (byte)0x69, (byte)0x38, (byte)0x5c, (byte)0x6e, (byte)0x61, (byte)0xc9,
      (byte)0xac, (byte)0x6c, (byte)0x66, (byte)0x23, (byte)0x67, (byte)0x5d, (byte)0xe9, (byte)0xae, (byte)0xf4,
      (byte)0x9b, (byte)0x52, (byte)0x43, (byte)0x43, (byte)0x45, (byte)0xba, (byte)0x65, (byte)0xbb, (byte)0x6b,
      (byte)0x2c, (byte)0xe6, (byte)0xc7, (byte)0x2c, (byte)0xcd, (byte)0x8f, (byte)0xc6, (byte)0x30, (byte)0x5e,
      (byte)0xee, (byte)0x3b, (byte)0xda, (byte)0xac, (byte)0xb5, (byte)0x79, (byte)0x6c, (byte)0xaf, (byte)0x7d,
      (byte)0xf6, (byte)0xfe, (byte)0xe3, (byte)0xf5, (byte)0xfa, (byte)0x30, (byte)0x11, (byte)0x1b, (byte)0xcc,
      (byte)0x38, (byte)0x05, (byte)0x9b, (byte)0xce, (byte)0x42, (byte)0x5e, (byte)0x29, (byte)0x14, (byte)0x9c,
      (byte)0x81, (byte)0x0c, (byte)0x07, (byte)0x44, (byte)0xf3, (byte)0x3f, (byte)0x05, (byte)0x10, (byte)0xea,
      (byte)0x80, (byte)0xa2, (byte)0x52, (byte)0xc3, (byte)0x18, (byte)0x75, (byte)0x2f, (byte)0x59, (byte)0xa2,
      (byte)0xbb, (byte)0x99, (byte)0x99, (byte)0xea, (byte)0xca, (byte)0xce, (byte)0x56, (byte)0x63, (byte)0x72,
      (byte)0xb2, (byte)0xca, (byte)0x42, (byte)0x42, (byte)0x7e, (byte)0x16, (byte)0x83, (byte)0xeb, (byte)0x38,
      (byte)0xc4, (byte)0x30, (byte)0x91, (byte)0x43, (byte)0x10, (byte)0x79, (byte)0x0a, (byte)0x7a, (byte)0x2b,
      (byte)0xa3, (byte)0xa3, (byte)0xe5, (byte)0x2f, (byte)0x2a, (byte)0xd2, (byte)0x1b, (byte)0xa7, (byte)0x53,
      (byte)0x6f, (byte)0xff, (byte)0xc9, (byte)0x8b, (byte)0xf2, (byte)0x72, (byte)0x35, (byte)0x06, (byte)0x46,
      (byte)0x4b, (byte)0x60, (byte)0xd8, (byte)0x0e, (byte)0x89, (byte)0xfc, (byte)0x6b, (byte)0x03, (byte)0x4c,
      (byte)0x2a, (byte)0x86, (byte)0x4e, (byte)0x57, (byte)0x78, (byte)0xb8, (byte)0x7c, (byte)0x05, (byte)0xf9,
      (byte)0x7a, (byte)0x56, (byte)0x74, (byte)0x44, (byte)0x43, (byte)0x8e, (byte)0xa3, (byte)0x1a, (byte)0x3a,
      (byte)0x69, (byte)0xd3, (byte)0xd0, (byte)0x09, (byte)0x9b, (byte)0x9e, (byte)0xdb, (byte)0x6c, (byte)0xf2,
      (byte)0x1f, (byte)0x3e, (byte)0xac, (byte)0xc1, (byte)0x40, (byte)0x9a, (byte)0xe2, (byte)0xe2, (byte)0xcc,
      (byte)0x91, (byte)0x77, (byte)0x47, (byte)0x21, (byte)0x8a, (byte)0x31, (byte)0xc7, (byte)0x20, (byte)0xfb,
      (byte)0x22, (byte)0xe8, (byte)0xc1, (byte)0xda, (byte)0x24, (byte)0xf5, (byte)0x15, (byte)0xee, (byte)0xd5,
      (byte)0x97, (byte)0x57, (byte)0x2f, (byte)0xe4, (byte)0x75, (byte)0xe4, (byte)0xc8, (byte)0x7f, (byte)0x30,
      (byte)0x43, (byte)0x83, (byte)0xb9, (byte)0x99, (byte)0xea, (byte)0xde, (byte)0x93, (byte)0xa5, (byte)0x4f,
      (byte)0x7e, (byte)0xbf, (byte)0xbc, (byte)0x76, (byte)0xbb, (byte)0x7a, (byte)0x52, (byte)0x53, (byte)0xe5,
      (byte)0xb4, (byte)0x58, (byte)0xe4, (byte)0x80, (byte)0xcb, (byte)0x40, (byte)0x10, (byte)0x80, (byte)0xe5,
      (byte)0x34, (byte)0xf4, (byte)0xd6, (byte)0x58, (byte)0x0c, (byte)0x3d, (byte)0x4a, (byte)0x5b, (byte)0xa6,
      (byte)0xee, (byte)0x92, (byte)0x42, (byte)0x99, (byte)0xbe, (byte)0x7d, (byte)0xfe, (byte)0xa8, (byte)0x3b,
      (byte)0x79, (byte)0x69, (byte)0x6a, (byte)0xdb, (byte)0xbe, (byte)0x42, (byte)0x23, (byte)0xfe, (byte)0x01,
      (byte)0x99, (byte)0x06, (byte)0xeb, (byte)0x6b, (byte)0xd5, (byte)0xb6, (byte)0x34, (byte)0x51, (byte)0xcd,
      (byte)0x56, (byte)0xab, (byte)0xce, (byte)0xc1, (byte)0xb7, (byte)0x3c, (byte)0x98, (byte)0x4d, (byte)0x26,
      (byte)0x44, (byte)0x96, (byte)0xc1, (byte)0xaf, (byte)0xe6, (byte)0x10, (byte)0xd4, (byte)0x97, (byte)0x3c,
      (byte)0x45, (byte)0xf7, (byte)0x36, (byte)0x87, (byte)0xeb, (byte)0x4e, (byte)0x49, (byte)0x9e, (byte)0x4c,
      (byte)0x5f, (byte)0x3f, (byte)0x8e, (byte)0xe8, (byte)0x9d, (byte)0xcf, (byte)0x2b, (byte)0xd3, (byte)0xd3,
      (byte)0x26, (byte)0xb7, (byte)0x3c, (byte)0x09, (byte)0x73, (byte)0x74, (byte)0x37, (byte)0x76, (byte)0xba,
      (byte)0x5a, (byte)0x0d, (byte)0x74, (byte)0x09, (byte)0x74, (byte)0x12, (byte)0x36, (byte)0x91, (byte)0x0f,
      (byte)0x8b, (byte)0xcd, (byte)0xe3, (byte)0x96, (byte)0x15, (byte)0x3d, (byte)0x59, (byte)0x8d, (byte)0x7c,
      (byte)0x19, (byte)0xa8, (byte)0x23, (byte)0x1d, (byte)0x35, (byte)0x14, (byte)0x17, (byte)0x6a, (byte)0x4c,
      (byte)0x7f, (byte)0x5b, (byte)0xb3, (byte)0x2a, (byte)0x56, (byte)0x06, (byte)0xeb, (byte)0x49, (byte)0x0a,
      (byte)0x7a, (byte)0xb8, (byte)0x08, (byte)0xb5, (byte)0x87, (byte)0x21, (byte)0x27, (byte)0xe8, (byte)0x0c,
      (byte)0xec, (byte)0x61, (byte)0x3f, (byte)0x2c, (byte)0xbc, (byte)0x0a, (byte)0x6a, (byte)0x0b, (byte)0x43,
      (byte)0x8f, (byte)0x57, (byte)0xa1, (byte)0x81, (byte)0x40, (byte)0xd9, (byte)0x93, (byte)0x16, (byte)0xa1,
      (byte)0x97, (byte)0xfd, (byte)0x3d, (byte)0x1a, (byte)0x33, (byte)0x3a, (byte)0xf2, (byte)0x5e, (byte)0xd5,
      (byte)0x59, (byte)0xf1, (byte)0xea, (byte)0xdb, (byte)0x82, (byte)0xfa, (byte)0x92, (byte)0x50, (byte)0x67,
      (byte)0x24, (byte)0xaa, (byte)0x02, (byte)0x9d, (byte)0x83, (byte)0x9d, (byte)0xcc, (byte)0x03, (byte)0xeb,
      (byte)0x35, (byte)0x18, (byte)0x6d, (byte)0x9c, (byte)0x84, (byte)0xba, (byte)0x16, (byte)0xa3, (byte)0xfa,
      (byte)0x75, (byte)0x11, (byte)0x7a, (byte)0x3b, (byte)0xf0, (byte)0xa7, (byte)0x3c, (byte)0xd4, (byte)0x55,
      (byte)0xad, (byte)0x9e, (byte)0x2b, (byte)0x39, (byte)0x32, (byte)0x7d, (byte)0xf9, (byte)0xf0, (byte)0x5e,
      (byte)0xad, (byte)0x3b, (byte)0xe3, (byte)0xd5, (byte)0x9d, (byte)0x82, (byte)0x6e, (byte)0x85, (byte)0xa3,
      (byte)0x6a, (byte)0x90, (byte)0x03, (byte)0x56, (byte)0x00, (byte)0x4c, (byte)0xbe, (byte)0x04, (byte)0xb5,
      (byte)0x1e, (byte)0x50, (byte)0xdd, (byte)0x1c, (byte)0xd4, (byte)0xed, (byte)0xc8, (byte)0x95, (byte)0xe9,
      (byte)0xf5, (byte)0xbd, (byte)0x6a, (byte)0xdd, (byte)0xdf, (byte)0x1b, (byte)0x2c, (byte)0xef, (byte)0x41,
      (byte)0xe4, (byte)0xab, (byte)0xdc, (byte)0x2f, (byte)0xd3, (byte)0x9b, (byte)0x07, (byte)0x4d, (byte)0x6a,
      (byte)0x4e, (byte)0x44, (byte)0x75, (byte)0x16, (byte)0xe4, (byte)0x82, (byte)0x57, (byte)0xb1, (byte)0x10,
      (byte)0x0a, (byte)0x10, (byte)0xb4, (byte)0x0f, (byte)0x92, (byte)0xea, (byte)0xe1, (byte)0xc7, (byte)0x75,
      (byte)0x03, (byte)0x55, (byte)0x45, (byte)0x05, (byte)0xe9, (byte)0xf1, (byte)0x91, (byte)0x1d, (byte)0x6a,
      (byte)0xdf, (byte)0x18, (byte)0xac, (byte)0xde, (byte)0x6d, (byte)0xc8, (byte)0xbb, (byte)0x1b, (byte)0xf5,
      (byte)0xee, (byte)0x42, (byte)0xbe, (byte)0xf3, (byte)0xe9, (byte)0xea, (byte)0x4c, (byte)0xb5, (byte)0xca,
      (byte)0x63, (byte)0x45, (byte)0x37, (byte)0x40, (byte)0x17, (byte)0xe0, (byte)0x00, (byte)0x60, (byte)0xf0,
      (byte)0xd7, (byte)0xd4, (byte)0x32, (byte)0xb0, (byte)0xb7, (byte)0x80, (byte)0x5c, (byte)0x06, (byte)0xaa,
      (byte)0x9c, (byte)0x85, (byte)0xea, (byte)0x16, (byte)0xa0, (byte)0xd6, (byte)0xe5, (byte)0xe8, (byte)0x76,
      (byte)0x0a, (byte)0xea, (byte)0x58, (byte)0x8f, (byte)0x9a, (byte)0x12, (byte)0x02, (byte)0xe3, (byte)0xa1,
      (byte)0xa8, (byte)0x01, (byte)0xe4, (byte)0x86, (byte)0x16, (byte)0x2b, (byte)0x84, (byte)0x31, (byte)0xce,
      (byte)0x8c, (byte)0x72, (byte)0xb0, (byte)0xdf, (byte)0x86, (byte)0xef, (byte)0xb5, (byte)0x20, (byte)0xa7,
      (byte)0x81, (byte)0x2a, (byte)0x42, (byte)0x02, (byte)0x09, (byte)0x94, (byte)0x9c, (byte)0x93, (byte)0x51,
      (byte)0x0d, (byte)0xa8, (byte)0x1d, (byte)0xe4, (byte)0x01, (byte)0x77, (byte)0x3c, (byte)0xcc, (byte)0x05,
      (byte)0x82, (byte)0x98, (byte)0xc0, (byte)0xb4, (byte)0x6c, (byte)0x58, (byte)0xe3, (byte)0x02, (byte)0x77,
      (byte)0x0b, (byte)0x0c, (byte)0xb7, (byte)0x82, (byte)0x02, (byte)0xaf, (byte)0x6e, (byte)0xc2, (byte)0x68,
      (byte)0x1d, (byte)0x74, (byte)0xd8, (byte)0x61, (byte)0xab, (byte)0xf9, (byte)0xd1, (byte)0xf8, (byte)0xf2,
      (byte)0x78, (byte)0x06, (byte)0x30, (byte)0x1d, (byte)0x88, (byte)0x99, (byte)0x0f, (byte)0xf1, (byte)0x89,
      (byte)0x90, (byte)0x60, (byte)0x40, (byte)0x2c, (byte)0x30, (byte)0x13, (byte)0xb0, (byte)0x30, (byte)0xce,
      (byte)0x6f, (byte)0x32, (byte)0x90, (byte)0x99, (byte)0xe7, (byte)0x97, (byte)0x18, (byte)0x51, (byte)0x1c,
      (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x49, (byte)0x45, (byte)0x4e, (byte)0x44, (byte)0xae,
      (byte)0x42, (byte)0x60, (byte)0x82
    };

    // load image data
    try
    {
      ByteArrayInputStream inputStream = new ByteArrayInputStream(IMAGE_CLOSE_DATA_ARRAY);
      IMAGE_CLOSE_DATA = new ImageData(inputStream);
      inputStream.close();
    }
    catch (Exception exception)
    {
      throw new Error(exception);
    }
  }

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
    TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
    if (tableLayoutData != null)
    {
      tableLayoutData.row         = row;
      tableLayoutData.column      = column;
      tableLayoutData.style       = style;
      tableLayoutData.rowSpawn    = Math.max(1,rowSpawn);
      tableLayoutData.columnSpawn = Math.max(1,columnSpawn);
      tableLayoutData.padX        = padX;
      tableLayoutData.padY        = padY;
      tableLayoutData.width       = width;
      tableLayoutData.height      = height;
      tableLayoutData.minWidth    = minWidth;
      tableLayoutData.minHeight   = minHeight;
      tableLayoutData.maxWidth    = maxWidth;
      tableLayoutData.maxHeight   = maxHeight;
    }
    else
    {
      tableLayoutData = new TableLayoutData(row,
                                            column,
                                            style,
                                            rowSpawn,
                                            columnSpawn,
                                            padX,
                                            padY,
                                            width,
                                            height,
                                            minWidth,
                                            minHeight,
                                            maxWidth,
                                            maxHeight
                                           );
    }
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
   * @return size [w,h] of text
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
   * @return max. size [w,h] of all texts
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
   * @return max. size [w,h] of all texts
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

  /** render text into an image
   * @param
   */
  public static void textToImage(Image image, GC fromGC, String text, int x, int y)
  {
    GC gc = new GC(image);

    gc.drawText(text,x,y,true);
    gc.setForeground(fromGC.getForeground());
    gc.setBackground(fromGC.getBackground());
    gc.dispose();
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

      keyCode = Character.toLowerCase(keyCode);
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
   * @param isVisible true to make visible, false to make invisible
   */
  public static void setVisible(Control control, boolean isVisible)
  {
    if (!control.isDisposed())
    {
      TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
      if (tableLayoutData != null)
      {
        tableLayoutData.isVisible = isVisible;
      }
      else
      {
        control.setLayoutData(new TableLayoutData(isVisible));
      }
      control.setVisible(isVisible);
      if (isVisible)
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
      control.forceFocus();
      if      (control instanceof Button)
      {
      }
      else if (control instanceof Combo)
      {
        Combo widget = (Combo)control;
        String text  = widget.getText();
        widget.setSelection(new Point(0,text.length()));
      }
      else if (control instanceof List)
      {
      }
      else if (control instanceof Spinner)
      {
      }
      else if (control instanceof Text)
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
      else if (control instanceof Table)
      {
      }
      else if (control instanceof Tree)
      {
      }
      else if (control instanceof DateTime)
      {
      }
      else if (control instanceof Composite)
      {
        Composite composite = (Composite)control;
        composite.setFocus();
      }
      else
      {
        throw new Error("Internal error: unknown control in setFocus(): "+control);
      }
    }
  }

  /** set next focus for controls
   * @param controls controls
   */
  public static void setNextFocus(final Control... controls)
  {
    // add selection listeners
    for (int i = 0; i < controls.length-1; i++)
    {
      if (!controls[i].isDisposed())
      {
        final Control nextControl = controls[i+1];
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
        if      (controls[i] instanceof Composite)
        {
          TypedListener typedListener = new TypedListener(selectionListener);
          ((Composite)controls[i]).addListener(SWT.Selection, typedListener);
          ((Composite)controls[i]).addListener(SWT.DefaultSelection, typedListener);
        }
        else if (controls[i] instanceof Button)
        {
          ((Button)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof Combo)
        {
          ((Combo)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof Spinner)
        {
          ((Spinner)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof Text)
        {
          ((Text)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof StyledText)
        {
          ((StyledText)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof Table)
        {
          ((Table)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof Tree)
        {
          ((Tree)controls[i]).addSelectionListener(selectionListener);
        }
        else if (controls[i] instanceof DateTime)
        {
          ((DateTime)controls[i]).addSelectionListener(selectionListener);
        }
        else
        {
          throw new Error("Internal error: unknown control in setNextFocus(): "+controls[i]);
        }

        /*
does not work on Windows? Even cursor keys trigger traversal event?
        controls[i].addTraverseListener(new TraverseListener()
        {
          public void keyTraversed(TraverseEvent traverseEvent)
          {
            Widgets.setFocus(nextControl);
            traverseEvent.doit = false;
          }
        });
        */
      }
    }

    // set tab traversal
    LinkedList<Control> controlList = new LinkedList(Arrays.asList(controls));
    while (controlList.size() > 1)
    {
      int n = controlList.size();
//Dprintf.dprintf("controls %d:",controlList.size()); for (Control control : controlList) { Dprintf.dprintf("  %s",control); } Dprintf.dprintf("");

      // find most left and deepest control in widget tree
      int i        = 0;
      int maxLevel = 0;
      for (int j = 0; j < controlList.size(); j++)
      {
        Control control = controlList.get(j);

        int level = 0;
        while (control.getParent() != null)
        {
          level++;
          control = control.getParent();
        }
        if (level > maxLevel)
        {
          i        = j;
          maxLevel = level;
        }
      }

      // get parent composite
      Composite parentComposite = controlList.get(i).getParent();

      if ((i < controlList.size()-1) && (controlList.get(i+1).getParent() == parentComposite))
      {
        // get all consecutive controls with same parent composite
        ArrayList<Control> tabControlList = new ArrayList<Control>();
        tabControlList.add(controlList.get(i)); controlList.remove(i);
        while ((i < controlList.size()) && (controlList.get(i).getParent() == parentComposite))
        {
          tabControlList.add(controlList.get(i)); controlList.remove(i);
        }

        // set tab control
        Control[] tabControls = tabControlList.toArray(new Control[tabControlList.size()]);
        parentComposite.setTabList(tabControls);
//Dprintf.dprintf("  tabControls: %d",tabControls.length); for (Control control : tabControls) { Dprintf.dprintf("    %s",control); } Dprintf.dprintf("");

        // replace by parent composite
        controlList.add(i,parentComposite);
      }
      else
      {
//Dprintf.dprintf("  remove: %s",controlList.get(i));
        controlList.remove(i);
      }
//Dprintf.dprintf("---");

      assert controlList.size() < n;
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
          if (!control.isDisposed())
          {
            control.setCursor(cursor);
          }
        }
      });
    }
  }

  /** reset cursor for control to default cursor
   * @param control control
   */
  public static void resetCursor(Control control)
  {
    setCursor(control,null);
  }

  /** check if widget is child of composite
   * @param composite composite
   * @param widget widget to check
   * @return true iff widget is child of composite
   */
  public static boolean isChildOf(Composite composite, Widget widget)
  {
    if (widget instanceof Control)
    {
      Control control = (Control)widget;
      while (   (control != null)
             && (composite != control)
             && (composite != control.getParent())
            )
      {
        control = control.getParent();
      }

      return (control != null);
    }
    else
    {
      return false;
    }
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

  /** get data from widget
   * @param widget widget
   * @param defaultValue default data value
   * @return data value
   */
  public static <T> T getData(Widget widget, T defaultValue)
  {
    T value;

    Object data = widget.getData();
    if (data != null)
    {
      value = (T)data;
    }
    else
    {
      value = defaultValue;
    }

    return value;
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
   * @param isVisible true for visible, false otherwise
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style, int accelerator, boolean isVisible)
  {
    Label label;

    if (accelerator != SWT.NONE)
    {
      char key = Character.toLowerCase((char)(accelerator & SWT.KEY_MASK));
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
    label.setLayoutData(new TableLayoutData(isVisible));
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
   * @param accelerator accelerator key code or SWT.NONE
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style, int accelerator)
  {
    return newLabel(composite,text,style,accelerator,true);
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param style label style
   * @param isVisible true for visible, false otherwise
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style, boolean isVisible)
  {
    return newLabel(composite,text,style,SWT.NONE,isVisible);
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param style label style
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, int style)
  {
    return newLabel(composite,text,style,true);
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param isVisible true for visible, false otherwise
   * @return new label
   */
  public static Label newLabel(Composite composite, String text, boolean isVisible)
  {
    return newLabel(composite,text,SWT.LEFT,isVisible);
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @return new label
   */
  public static Label newLabel(Composite composite, String text)
  {
    return newLabel(composite,text,true);
  }

  /** create new label
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new label
   */
  public static Label newLabel(Composite composite, boolean isVisible)
  {
    return newLabel(composite,"",isVisible);
  }

  /** create new label
   * @param composite composite widget
   * @return new label
   */
  public static Label newLabel(Composite composite)
  {
    return newLabel(composite,true);
  }

  //-----------------------------------------------------------------------

  /** create new image
   * @param composite composite widget
   * @param image image
   * @param style label style
   * @return new image
   */
  public static Label newImage(Composite composite, Image image, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setImage(image);

    return label;
  }

  /** create new image
   * @param composite composite widget
   * @param image image
   * @return new image
   */
  public static Label newImage(Composite composite, Image image)
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
    return newNumberView(composite,SWT.RIGHT|SWT.BORDER);
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
   * @param isVisible true for visible, false otherwise
   * @return new text view
   */
  public static StyledText newTextView(Composite composite, int style, boolean isVisible)
  {
    StyledText styledText;

    styledText = new StyledText(composite,style|SWT.READ_ONLY);
    styledText.setLayoutData(new TableLayoutData(isVisible));
    styledText.setBackground(composite.getBackground());
    styledText.setText("");

    return styledText;
  }

  /** create new text view
   * @param composite composite widget
   * @param style view style
   * @return new text view
   */
  public static StyledText newTextView(Composite composite, int style)
  {
    return newTextView(composite,style,true);
  }

  /** create new string view
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new view
   */
  public static StyledText newTextView(Composite composite, boolean isVisible)
  {
    return newTextView(composite,SWT.LEFT|SWT.BORDER|SWT.MULTI|SWT.H_SCROLL|SWT.V_SCROLL,isVisible);
  }

  /** create new string view
   * @param composite composite widget
   * @return new view
   */
  public static StyledText newTextView(Composite composite)
  {
    return newTextView(composite,true);
  }

  //-----------------------------------------------------------------------

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @param accelerator accelerator key code or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, String text, int style, int accelerator, boolean isVisible)
  {
    Button button;

    if (accelerator != SWT.NONE)
    {
      char key = Character.toLowerCase((char)(accelerator & SWT.KEY_MASK));
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
    button = new Button(composite,style|SWT.PUSH);
    button.setLayoutData(new TableLayoutData(isVisible));
    button.setText(text);

    return button;
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @param accelerator accelerator key code or SWT.NONE
   * @return new button
   */
  public static Button newButton(Composite composite, String text, int style, int accelerator)
  {
    return newButton(composite,text,style,accelerator,true);
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, String text, int style, boolean isVisible)
  {
    return newButton(composite,text,style,SWT.NONE,isVisible);
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @return new button
   */
  public static Button newButton(Composite composite, String text, int style)
  {
    return newButton(composite,text,style,true);
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, String text, boolean isVisible)
  {
    return newButton(composite,text,SWT.NONE,isVisible);
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @param style SWT style flags
   * @return new button
   */
  public static Button newButton(Composite composite, String text)
  {
    return newButton(composite,text,true);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, int style, boolean isVisible)
  {
    Button button;

    button = new Button(composite,style|SWT.PUSH);
    button.setLayoutData(new TableLayoutData(isVisible));
    button.setImage(image);

    return button;
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param style SWT style flags
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, int style)
  {
    return newButton(composite,image,style,true);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, boolean isVisible)
  {
    return newButton(composite,image,SWT.NONE,isVisible);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @return new button
   */
  public static Button newButton(Composite composite, Image image)
  {
    return newButton(composite,image,true);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param text text
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, String text, int style, boolean isVisible)
  {
    Button button;

    button = new Button(composite,style|SWT.PUSH);
    button.setLayoutData(new TableLayoutData(isVisible));
    button.setImage(image);
    button.setText(text);

    return button;
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param text text
   * @param style SWT style flags
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, String text, int style)
  {
    return newButton(composite,image,text,style,true);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param text text
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, String text, boolean isVisible)
  {
    return newButton(composite,image,text,SWT.NONE,isVisible);
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @param text text
   * @return new button
   */
  public static Button newButton(Composite composite, Image image, String text)
  {
    return newButton(composite,image,text,true);
  }

  /** create new button
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newButton(Composite composite, boolean isVisible)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setLayoutData(new TableLayoutData(isVisible));

    return button;
  }

  /** create new button
   * @param composite composite widget
   * @return new button
   */
  public static Button newButton(Composite composite)
  {
    return newButton(composite,true);
  }

  //-----------------------------------------------------------------------

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param accelerator accelerator key code or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, final Object data, final String field, int accelerator, boolean isVisible)
  {
    Button button;

    if (accelerator != SWT.NONE)
    {
      char key = Character.toLowerCase((char)(accelerator & SWT.KEY_MASK));
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
    button.setLayoutData(new TableLayoutData(isVisible));
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

    return button;
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, Object data, String field, int accelerator)
  {
    return newCheckbox(composite,text,data,field,accelerator,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, Object data, String field, boolean isVisible)
  {
    return newCheckbox(composite,text,data,field,SWT.NONE,isVisible);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, Object data, String field)
  {
    return newCheckbox(composite,text,data,field,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param accelerator accelerator key code or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, int accelerator, boolean isVisible)
  {
    return newCheckbox(composite,text,(Object)null,(String)null,accelerator,isVisible);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, int accelerator)
  {
    return newCheckbox(composite,text,accelerator,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text, boolean isVisible)
  {
    return newCheckbox(composite,text,SWT.NONE,isVisible);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, String text)
  {
    return newCheckbox(composite,text,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, boolean isVisible)
  {
    return newCheckbox(composite,(String)null,isVisible);
  }

  /** create new checkbox
   * @param composite composite widget
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite)
  {
    return newCheckbox(composite,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param imageOn,imageOf on/off image
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, final Image imageOn, final Image imageOff, final Object data, final String field, boolean isVisible)
  {
    Button button;

    button = new Button(composite,SWT.TOGGLE);
    button.setLayoutData(new TableLayoutData(isVisible));
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

    return button;
  }

  /** create new checkbox
   * @param composite composite widget
   * @param accelerator accelerator key code or SWT.NONE
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, Image imageOff, Image imageOn, Object data, String field)
  {
    return newCheckbox(composite,imageOff,imageOn,data,field,true);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param imageOff,imageOn off/on image
   * @param isVisible true for visible, false otherwise
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, Image imageOff, Image imageOn, boolean isVisible)
  {
    return newCheckbox(composite,imageOff,imageOn,(Object)null,(String)null,isVisible);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param imageOff,imageOn off/on image
   * @return new checkbox button
   */
  public static Button newCheckbox(Composite composite, Image imageOff, Image imageOn)
  {
    return newCheckbox(composite,imageOff,imageOn,true);
  }

  //-----------------------------------------------------------------------

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newRadio(Composite composite, String text, final Object data, final String field, final Object value, boolean isVisible)
  {
    Button button;

    button = new Button(composite,SWT.RADIO);
    button.setLayoutData(new TableLayoutData(isVisible));
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
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new button
   */
  public static Button newRadio(Composite composite, String text, final Object data, final String field, final Object value)
  {
    return newRadio(composite,text,data,field,value,true);
  }

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newRadio(Composite composite, String text, boolean isVisible)
  {
    return newRadio(composite,text,(Object)null,(String)null,(Object)null,isVisible);
  }

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @param isVisible true for visible, false otherwise
   * @return new button
   */
  public static Button newRadio(Composite composite, String text)
  {
    return newRadio(composite,text,true);
  }

  //-----------------------------------------------------------------------

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @param style text style
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, final Object data, final String field, String value, int style, boolean isVisible)
  {
    Text text;

    text = new Text(composite,style);
    text.setLayoutData(new TableLayoutData(isVisible));
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
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field, String value, boolean isVisible)
  {
    return newText(composite,data,field,value,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE,isVisible);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field, String value)
  {
    return newText(composite,data,field,value,true);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field, int style, boolean isVisible)
  {
    return newText(composite,data,field,"",style,isVisible);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field, int style)
  {
    return newText(composite,data,field,style,true);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field, boolean isVisible)
  {
    return newText(composite,data,field,"",isVisible);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new text widget
   */
  public static Text newText(Composite composite, Object data, String field)
  {
    return newText(composite,data,field,true);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, int style, boolean isVisible)
  {
    return newText(composite,(String)null,(String)null,style,isVisible);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @return new text widget
   */
  public static Text newText(Composite composite, int style)
  {
    return newText(composite,style,true);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new text widget
   */
  public static Text newText(Composite composite, boolean isVisible)
  {
    return newText(composite,(String)null,(String)null,isVisible);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  public static Text newText(Composite composite)
  {
    return newText(composite,true);
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
  public static StyledText newStyledText(Composite composite, Object data, String field, String value)
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
  public static StyledText newStyledText(Composite composite, Object data, String field, int style)
  {
    return newStyledText(composite,data,field,"",style);
  }

  /** create new styled text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new text widget
   */
  public static StyledText newStyledText(Composite composite, Object data, String field)
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

  /** create new date input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for date input field
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite, final Object data, final String field, Date value, int style)
  {
    DateTime dateTime;

    dateTime = new DateTime(composite,style|SWT.BORDER|SWT.DATE|SWT.DROP_DOWN);
    if      (value != null)
    {
      setDate(dateTime,value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      setDate(dateTime,value);
    }

    dateTime.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        DateTime widget = (DateTime)selectionEvent.widget;
        setField(data,field,getDate(widget));
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    dateTime.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
      }
      public void focusLost(FocusEvent focusEvent)
      {
        DateTime widget = (DateTime)focusEvent.widget;
        setField(data,field,getDate(widget));
      }
    });

    return dateTime;
  }

  /** create new date input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for date input field
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite, Object data, String field, Date value)
  {
    return newDate(composite,data,field,value,SWT.NONE);
  }

  /** create new date input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite, Object data, String field, int style)
  {
    return newDate(composite,data,field,null,style);
  }

  /** create new date input widget
   * @param composit        setField(data,field,getTime(widget));
e composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite, Object data, String field)
  {
    return newDate(composite,data,field,null,SWT.NONE);
  }

  /** create new password input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite, int style)
  {
    return newDate(composite,null,null,style);
  }

  /** create new date input widget
   * @param composite composite widget
   * @return new date/time widget
   */
  public static DateTime newDate(Composite composite)
  {
    return newDate(composite,SWT.NONE);
  }

  /** get date from date/time widget
   * @param dateTime date/time widget
   * @return date
   */
  public static Date getDate(DateTime dateTime)
  {
    Calendar calendar = Calendar.getInstance();
    calendar.set(dateTime.getYear(),dateTime.getMonth(),dateTime.getDay());

    return calendar.getTime();
  }

  /** set date in date/time widget
   * @param dateTime date/time widget
   * @param date date
   */
  public static void setDate(DateTime dateTime, Date date)
  {
    Calendar calendar = Calendar.getInstance();
    calendar.setTime(date);

    dateTime.setDay  (calendar.get(Calendar.DAY_OF_MONTH));
    dateTime.setMonth(calendar.get(Calendar.MONTH       ));
    dateTime.setYear (calendar.get(Calendar.YEAR        ));
  }

  //-----------------------------------------------------------------------

  /** create new time input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for time input field
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite, final Object data, final String field, Date value, int style)
  {
    DateTime dateTime;

    dateTime = new DateTime(composite,style|SWT.TIME);
    if      (value != null)
    {
      setTime(dateTime,value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      setTime(dateTime,(Date)getField(data,field));
    }

    dateTime.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        DateTime widget = (DateTime)selectionEvent.widget;
        setField(data,field,getTime(widget));
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    dateTime.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
      }
      public void focusLost(FocusEvent focusEvent)
      {
        DateTime widget = (DateTime)focusEvent.widget;
        setField(data,field,getTime(widget));
      }
    });

    return dateTime;
  }

  /** create new time input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for time input field
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite, Object data, String field, Date value)
  {
    return newTime(composite,data,field,value,SWT.NONE);
  }

  /** create new time input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite, Object data, String field, int style)
  {
    return newTime(composite,data,field,null,style);
  }

  /** create new time input widget
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite, Object data, String field)
  {
    return newTime(composite,data,field,null,SWT.NONE);
  }

  /** create new time input widget
   * @param composite composite widget
   * @param style text style
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite, int style)
  {
    return newTime(composite,null,null,style);
  }

  /** create new time input widget
   * @param composite composite widget
   * @return new date/time widget
   */
  public static DateTime newTime(Composite composite)
  {
    return newTime(composite,SWT.NONE);
  }

  /** get date from date/time widget
   * @param dateTime date/time widget
   * @return date
   */
  public static Date getTime(DateTime dateTime)
  {
    Calendar calendar = Calendar.getInstance();
    calendar.set(0,0,0,dateTime.getHours(),dateTime.getMinutes(),dateTime.getSeconds());

    return calendar.getTime();
  }

  /** set date in date/time widget
   * @param dateTime date/time widget
   * @param date date
   */
  public static void setTime(DateTime dateTime, Date date)
  {
    Calendar calendar = Calendar.getInstance();
    calendar.setTime(date);

    dateTime.setHours  (calendar.get(Calendar.HOUR  ));
    dateTime.setMinutes(calendar.get(Calendar.MINUTE));
    dateTime.setSeconds(calendar.get(Calendar.SECOND));
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
    list.setData(new ArrayList<ListItem>());

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
  public static void sortList(List list, Comparator<ListItem> comparator)
  {
    if (!list.isDisposed())
    {
      ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
      Collections.sort(listItems,comparator);
      for (int i = 0; i < listItems.size(); i++)
      {
        list.setItem(i,listItems.get(i).text);
      }
    }
  }

  /** sort list by text
   * @param list list
   */
  public static void sortList(List list)
  {
    sortList(list,new Comparator<ListItem>()
    {
      public int compare(ListItem listItem1, ListItem listItem2)
      {
        return listItem1.text.compareTo(listItem2.text);
      }
    });
  }

  /** get insert position in sorted list
   * @param list list
   * @param data data
   * @param comparator data comparator
   * @return index in list
   */
  public static int getListItemIndex(List list, Object data, Comparator comparator)
  {
    int index = 0;

    if (!list.isDisposed())
    {
      ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

      for (int i = 0; i < listItems.size(); i++)
      {
        if (comparator != String.CASE_INSENSITIVE_ORDER)
        {
          if (comparator.compare(listItems.get(index).data,data) > 0)
          {
            index = i;
            break;
          }
        }
        else
        {
          if (comparator.compare(listItems.get(index).text,data) > 0)
          {
            index = i;
            break;
          }
        }
      }
    }

    return index;
  }

  /** get list item
   * @param list list
   * @param data data
   * @return index in list
   */
  public static int getListItemIndex(List list, Object data)
  {
    int index = 0;

    if (!list.isDisposed())
    {
      ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

      for (int i = 0; i < listItems.size(); i++)
      {
        if (listItems.get(index).data.equals(data))
        {
          index = i;
          break;
        }
      }
    }

    return index;
  }

  /** insert list item
   * @param list table
   * @param comparator list item comperator
   * @param list item data
   * @param text list text
   */
  public static ListItem insertListItem(final List list, final int index, final Object data, final String text)
  {
    if (!list.isDisposed())
    {
      final ListItem listItem = new ListItem(text,data);
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

          if (index >= 0)
          {
            list.add(text,index);
            listItems.add(index,listItem);
          }
          else
          {
            list.add(text);
            listItems.add(listItem);
          }
        }
      });
      return listItem;
    }
    else
    {
      return null;
    }

  }

  /** insert list item
   * @param list list
   * @param comparator list item comperator
   * @param list item data
   * @param text list text
   * @return list item
   * @return list item
   */
  public static ListItem insertListItem(final List list, final Comparator comparator, final Object data, final String text)
  {
    if (!list.isDisposed())
    {
      final ListItem listItem = new ListItem(text,data);
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

          int index = getListItemIndex(list,data,comparator);
          list.add(text,index);
          listItems.add(index,listItem);
        }
      });

      return listItem;
    }
    else
    {
      return null;
    }
  }

  /** add list item
   * @param list list
   * @param list item data
   * @param text list text
   */
  public static ListItem addListItem(List list, Object data, String text)
  {
    return insertListItem(list,-1,data,text);
  }

  /** update list item
   * @param list list
   * @param data item data
   * @param text item text
   * @param true if updated, false if not found
   */
  public static boolean updateListItem(final List list, final Object data, final String text)
  {
    /** list update runnable
     */
    class ListRunnable implements Runnable
    {
      boolean updatedFlag = false;

      public void run()
      {
        if (!list.isDisposed())
        {
          list.getDisplay().syncExec(new Runnable()
          {
            public void run()
            {
              ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

              for (int i = 0; i < listItems.size(); i++)
              {
                ListItem listItem = listItems.get(i);
                if (listItem.data.equals(data))
                {
                  listItem.text = text;
                  list.setItem(i,text);
                  updatedFlag = true;
                  break;
                }
              }
            }
          });
        }
      }
    }

    ListRunnable listRunnable = new ListRunnable();
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(listRunnable);
    }

    return listRunnable.updatedFlag;
  }

  /** swap list items
   * @param list list
   * @param i,j indizes of list items to swap
   */
  private static void swapListItems(List list, int i, int j)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

    String text0 = list.getItem(i);
    String text1 = list.getItem(j);
    list.setItem(i,text1);
    list.setItem(j,text0);

    ListItem listItem0 = listItems.get(i);
    ListItem listItem1 = listItems.get(j);
    listItems.set(i,listItem1);
    listItems.set(j,listItem0);

  }

  /** move list item
   * @param list list
   * @param data item data
   * @param offset move offset
   */
  public static void moveListItem(final List list, final int index, final int offset)
  {
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

            int i = index;
            int n = offset;

            // move item down
            while ((n > 0) && (i < listItems.size()-1))
            {
              swapListItems(list,i,i+1);
              i++;
              n--;
            }

            // move item up
            while ((n < 0) && (i > 0))
            {
              swapListItems(list,i,i-1);
              i--;
              n++;
            }
          }
        }
      });
    }
  }

  /** move list item
   * @param list list
   * @param data item data
   * @param offset move offset
   */
  public static void moveListItem(List list, Object data, int offset)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
    int                 i         = 0;
    while ((i < listItems.size()) && (listItems.get(i).data != data))
    {
      i++;
    }

    if (i >= 0)
    {
      moveListItem(list,i,offset);
    }
  }

  /** move list item
   * @param list list
   * @param data item data
   * @param offset move offset
   */
  public static void moveListItem(List list, ListItem listItem, int offset)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
    for (int i = 0; i < listItems.size(); i++)
    {
      if (listItems.get(i) == listItem)
      {
        moveListItem(list,i,offset);
        break;
      }
    }
  }

  /** remove list item
   * @param list list
   * @param list item data
   */
  public static void removeListItem(final List list, final int index)
  {
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
            list.remove(index);
            listItems.remove(index);
          }
        }
      });
    }
  }

  /** remove list item
   * @param list list
   * @param list item data
   */
  public static void removeListItem(final List list, final Object data)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
    for (int i = 0; i < listItems.size(); i++)
    {
      if (listItems.get(i).data.equals(data))
      {
        removeListItem(list,i);
        break;
      }
    }
  }

  /** remove list item
   * @param list list
   * @param listItem list item to remove
   */
  public static void removeListItem(final List list, final ListItem listItem)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
    for (int i = 0; i < listItems.size(); i++)
    {
      if (listItems.get(i) == listItem)
      {
        removeListItem(list,i);
        break;
      }
    }
  }

  /** remove list entries
   * @param list list
   * @param listItems list items to remove
   */
  public static void removeListEntries(List list, ListItem[] listItems)
  {
    for (ListItem listItem : listItems)
    {
      removeListItem(list,listItem);
    }
  }

  /** remove all list items
   * @param list list
   */
  public static void removeAllListItems(final List list)
  {
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            list.removeAll();
            ((ArrayList<ListItem>)list.getData()).clear();
          }
        }
      });
    }
  }

  /** get list item data
   * @param list list
   * @param index index
   * @return item data
   */
  public static Object getListItem(List list, int index)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

    return listItems.get(index).data;
  }

  /** get list items data
   * @param list list
   * @param entries array
   * @return items data array
   */
  public static <T> T[] getListItems(List list, T[] array)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

    if (array.length != listItems.size())
    {
      array = Arrays.copyOf(array,listItems.size());
    }

    for (int i = 0; i < listItems.size(); i++)
    {
      array[i] = (T)(listItems.get(i).data);
    }

    return array;
  }

  /** get list items data
   * @param list list
   * @param clazz class of array elements
   * @return items data array
   */
  public static <T> T[] getListItems(List list, Class clazz)
  {
    ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();

    T[] array = (T[])Array.newInstance(clazz,listItems.size());
    for (int i = 0; i < listItems.size(); i++)
    {
      array[i] = (T)listItems.get(i);
    }

    return array;
  }

  /** set selected list item
   * @param list list
   * @return selected list item data
   */
  public static <T> T getSelectedListItem(final List list)
  {
    final Object data[] = new Object[1];

    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
            data[0] = listItems.get(list.getSelectionIndex());
          }
        }
      });
    }

    return (T)data[0];
  }

  /** set selected list items
   * @param list list
   * @param clazz data type class
   * @return selected list items data
   */
  public static <T> T[] getSelectedListItems(final List list, Class clazz)
  {
    final ArrayList<Object> dataArray = new ArrayList<Object>();

    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
            for (int index : list.getSelectionIndices())
            {
              dataArray.add(listItems.get(index));
            }
          }
        }
      });
    }

    T[] array = (T[])Array.newInstance(clazz,dataArray.size());
    for (int i = 0; i < dataArray.size(); i++)
    {
      array[i] = (T)dataArray.get(i);
    }

    return array;
  }

  /** set selected list item
   * @param list list
   * @param data item data
   */
  public static <T> void setSelectedListItem(final List list, final T data)
  {
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            list.deselectAll();
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
            for (int i = 0; i < listItems.size(); i++)
            {
              if (listItems.get(i).equals(data))
              {
                list.select(i);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** set selected list items
   * @param list list
   * @param data items data
   */
  public static <T> void setSelectedListItems(final List list, final T data[])
  {
    if (!list.isDisposed())
    {
      list.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!list.isDisposed())
          {
            list.deselectAll();
            ArrayList<ListItem> listItems = (ArrayList<ListItem>)list.getData();
            for (T _data : data)
            {
              for (int i = 0; i < listItems.size(); i++)
              {
                if (listItems.get(i) == _data)
                {
                  list.select(i);
                  break;
                }
              }
            }
          }
        }
      });
    }
  }

  //-----------------------------------------------------------------------

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field, String value, int style, boolean isVisible)
  {
    Combo combo;

    combo = new Combo(composite,style);
    combo.setLayoutData(new TableLayoutData(isVisible));
    if      (value != null)
    {
      combo.setText(value);
      setField(data,field,value);
    }
    else if (getField(data,field) != null)
    {
      combo.setText((String)getField(data,field));
    }
    combo.setData(new ArrayList<Object>());

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
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, final Object data, final String field, String value, int style)
  {
    return newCombo(composite,data,field,value,style,true);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field, String value, boolean isVisible)
  {
    return newCombo(composite,data,field,value,SWT.BORDER,isVisible);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field, String value)
  {
    return newCombo(composite,data,field,value,true);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field, int style, boolean isVisible)
  {
    return newCombo(composite,data,field,(String)null,style,isVisible);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field, int style)
  {
    return newCombo(composite,data,field,style,true);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field, boolean isVisible)
  {
    return newCombo(composite,data,field,SWT.BORDER,isVisible);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, Object data, String field)
  {
    return newCombo(composite,data,field,true);
  }

  /** new combo widget
   * @param composite composite widget
   * @param style SWT style flags
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, int style, boolean isVisible)
  {
    return newCombo(composite,null,null,style,isVisible);
  }

  /** new combo widget
   * @param composite composite widget
   * @param style SWT style flags
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, int style)
  {
    return newCombo(composite,null,null,style,true);
  }

  /** new combo widget
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite, boolean isVisible)
  {
    return newCombo(composite,SWT.BORDER,isVisible);
  }

  /** new combo widget
   * @param composite composite widget
   * @return new combo widget
   */
  public static Combo newCombo(Composite composite)
  {
    return newCombo(composite,true);
  }

  /** set combo items
   * @param combo combo
   * @param items items (array of [text,data])
   */
  public static void setComboItems(final Combo combo, final Object items[])
  {
    assert (items.length % 2) == 0;

    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

          combo.removeAll();
          dataArray.clear();
          for (int i = 0; i < items.length/2; i++)
          {
            String text = (String)items[i*2+0];
            Object data = items[i*2+1];

            combo.add(text);
            dataArray.add(data);
          }
        }
      });
    }
  }

  /** get index of combo item
   * @param combo combo
   * @param data item data
   * @return index or -1
   */
  public static <T> int getComboIndex(final Combo combo, final T data)
  {
    final int index[] = new int[]{-1};

    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

          for (int i = 0; i < dataArray.size(); i++)
          {
            if (dataArray.get(i).equals(data))
            {
              index[0] = i;
              break;
            }
          }
        }
      });
    }

    return index[0];
  }

  /** insert combo item
   * @param combo combo
   * @param index insert index (0..n)
   * @param data item data
   * @param text item text
   */
  public static void insertComboItem(final Combo combo, final int index, final Object data, final String text)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

          combo.add(text,index);
          dataArray.add(index,data);
        }
      });
    }
  }

  /** add combo item
   * @param combo combo
   * @param data item data
   * @param text item text
   */
  public static void addComboItem(final Combo combo, final Object data, final String text)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

          combo.add(text);
          dataArray.add(data);
        }
      });
    }
  }

  /** update combo item
   * @param combo combo
   * @param index index (0..n-1)
   * @param data item data
   * @param text item text
   */
  public static void updateComboItem(final Combo combo, final int index, final Object data, final String text)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!combo.isDisposed())
          {
            combo.getDisplay().syncExec(new Runnable()
            {
              public void run()
              {
                ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

                if (!combo.getItem(index).equals(text)) combo.setItem(index,text);
                dataArray.set(index,data);
              }
            });
          }
        }
      });
    }
  }

  /** update combo item
   * @param combo combo
   * @param data item data
   * @param text item text
   * @param true if updated, false if not found
   */
  public static boolean updateComboItem(final Combo combo, final Object data, final String text)
  {
    /** combo update runnable
     */
    class ComboRunnable implements Runnable
    {
      boolean updatedFlag = false;

      public void run()
      {
        if (!combo.isDisposed())
        {
          combo.getDisplay().syncExec(new Runnable()
          {
            public void run()
            {
              ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

              for (int i = 0; i < dataArray.size(); i++)
              {
                if (dataArray.get(i).equals(data))
                {
                  combo.setItem(i,text);
                  updatedFlag = true;
                  break;
                }
              }
            }
          });
        }
      }
    }

    ComboRunnable comboRunnable = new ComboRunnable();
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(comboRunnable);
    }

    return comboRunnable.updatedFlag;
  }

  /** remove combo item
   * @param combo combo
   * @param index item index
   */
  public static void removeComboItem(final Combo combo, final int index)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!combo.isDisposed())
          {
            ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();
            combo.remove(index);
            dataArray.remove(index);
          }
        }
      });
    }
  }

  /** remove combo item
   * @param combo combo
   * @param combo item data
   */
  public static void removeComboItem(final Combo combo, final Object data)
  {
    ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();
    for (int i = 0; i < dataArray.size(); i++)
    {
      if (dataArray.get(i).equals(data))
      {
        removeComboItem(combo,i);
        break;
      }
    }
  }

  /** remove combo items
   * @param combo combo
   * @param data data of combo items to remove
   */
  public static void removeComboItems(Combo combo, Object[] data)
  {
    for (Object _data : data)
    {
      removeComboItem(combo,_data);
    }
  }

  /** remove all combo items
   * @param combo combo
   */
  public static void removeAllComboItems(final Combo combo)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!combo.isDisposed())
          {
            combo.removeAll();
            ((ArrayList<Object>)combo.getData()).clear();
          }
        }
      });
    }
  }

  /** get combo item
   * @param combo combo
   * @param index index
   * @return entry
   */
  public static Object getComboItem(Combo combo, int index)
  {
    ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

    return dataArray.get(index);
  }

  /** get combo items
   * @param combo combo
   * @param entries array
   * @return entries array
   */
  public static <T> T[] getComboItems(Combo combo, T[] array)
  {
    ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

    if (array.length != dataArray.size())
    {
      array = Arrays.copyOf(array,dataArray.size());
    }

    for (int i = 0; i < dataArray.size(); i++)
    {
      array[i] = (T)(dataArray.get(i));
    }

    return array;
  }

  /** get combo items data
   * @param combo combo
   * @param clazz data type class
   * @return data array
   */
  public static <T> T[] getComboItems(Combo combo, Class clazz)
  {
    ArrayList<Object> comboItems = (ArrayList<Object>)combo.getData();

    T[] array = (T[])Array.newInstance(clazz,comboItems.size());
    for (int i = 0; i < comboItems.size(); i++)
    {
      array[i] = (T)comboItems.get(i);
    }

    return array;
  }

  /** get selected combo item
   * @param combo combo
   * @param default default value
   * @return selected combo item data
   */
  public static <T> T getSelectedComboItem(final Combo combo, T defaultValue)
  {
    final Object data[] = new Object[]{defaultValue};

    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!combo.isDisposed())
          {
            ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();

            int index = combo.getSelectionIndex();
            if ((index >= 0) && (index < dataArray.size()))
            {
              data[0] = dataArray.get(index);
            }
          }
        }
      });
    }

    return (T)data[0];
  }

  /** set selected combo item
   * @param combo combo
   * @param data item data
   */
  public static <T> void setSelectedComboItem(final Combo combo, final T data)
  {
    if (!combo.isDisposed())
    {
      combo.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!combo.isDisposed())
          {
            ArrayList<Object> dataArray = (ArrayList<Object>)combo.getData();
            for (int i = 0; i < dataArray.size(); i++)
            {
              if (dataArray.get(i).equals(data))
              {
                combo.select(i);
                return;
              }
            }
            combo.setText(data.toString());
          }
        }
      });
    }
  }

  //-----------------------------------------------------------------------

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store select value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new select widget
   */
  public static Combo newEnumSelect(Composite composite, Class enumClass, final Object data, final String field, String value)
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
   * @param enumClass enum-class
   * @param data data structure to store select value or null
   * @param field field name in data structure to set on selection
   * @return new select widget
   */
  public static Combo newEnumSelect(Composite composite, Class enumClass, Object data, String field)
  {
    return newEnumSelect(composite,enumClass,data,field,null);
  }

  /** new enum select widget
   * @param composite composite widget
   * @param enumClass enum-class
   * @return new select widget
   */
  public static Combo newEnumSelect(Composite composite, Class enumClass)
  {
    return newEnumSelect(composite,enumClass,null,null);
  }

  //-----------------------------------------------------------------------

  /** create new option menu
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @param isVisible true for visible, false otherwise
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite, Object data, String field, String value, boolean isVisible)
  {
    return newCombo(composite,data,field,value,SWT.RIGHT|SWT.READ_ONLY,isVisible);
  }

  /** create new option menu
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite, Object data, String field, String value)
  {
    return newOptionMenu(composite,data,field,value,true);
  }

  /** create new option menu
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @param isVisible true for visible, false otherwise
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite, Object data, String field, boolean isVisible)
  {
    return newOptionMenu(composite,data,field,(String)null,isVisible);
  }

  /** create new option menu
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for combo
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite, Object data, String field)
  {
    return newOptionMenu(composite,data,field,true);
  }

  /** create new option menu
   * @param composite composite widget
   * @param isVisible true for visible, false otherwise
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite, boolean isVisible)
  {
    return newOptionMenu(composite,(Object)null,(String)null,isVisible);
  }

  /** create new option menu
   * @param composite composite widget
   * @return new option menu combo widget
   */
  public static Combo newOptionMenu(Composite composite)
  {
    return newOptionMenu(composite,true);
  }

  /** set option menu items
   * @param combo option menu combo
   * @param items items (array of [text,data])
   */
  public static void setOptionMenuItems(Combo combo, Object items[])
  {
    setComboItems(combo,items);
  }

  /** get insert position
   * @param combo option menu combo
   * @param data data
   * @return index
   */
  public static <T> int getOptionMenuIndex(Combo combo, T data)
  {
    return getComboIndex(combo,data);
  }

  /** insert option menu item
   * @param combo option menu combo
   * @param index index (0..n)
   * @param data item data
   * @param text item text
   */
  public static void insertOptionMenuItem(Combo combo, int index, Object data, String text)
  {
    insertComboItem(combo,index,data,text);
  }

  /** add option menu item
   * @param combo option menu combo
   * @param data item data
   * @param text item text
   */
  public static void addOptionMenuItem(Combo combo, Object data, String text)
  {
    addComboItem(combo,data,text);
  }

  /** add option menu item
   * @param combo option menu combo
   * @param data item data
   */
  public static void addOptionMenuItem(Combo combo, Object data)
  {
    addComboItem(combo,data,data.toString());
  }

  /** update option menu item
   * @param combo option menu combo
   * @param index index (0..n-1)
   * @param data item data
   * @param text item text
   * @param true if updated, false if not found
   */
  public static void updateOptionMenuItem(Combo combo, int index, Object data, String text)
  {
    updateComboItem(combo,index,data,text);
  }

  /** update option menu item
   * @param combo option menu combo
   * @param data item data
   * @param text item text
   * @param true if updated, false if not found
   */
  public static boolean updateOptionMenuItem(Combo combo, Object data, String text)
  {
    return updateComboItem(combo,data,text);
  }

  /** update option menu item
   * @param combo option menu combo
   * @param data item data
   * @param true if updated, false if not found
   */
  public static boolean updateOptionMenuItem(Combo combo, Object data)
  {
    return updateComboItem(combo,data,data.toString());
  }

  /** remove option menu item
   * @param combo option menu combo
   * @param index item index
   */
  public static void removeOptionMenuItem(Combo combo, int index)
  {
    removeComboItem(combo,index);
  }

  /** remove option menu item
   * @param combo option menu combo
   * @param combo item data
   */
  public static void removeOptionMenuItem(Combo combo, Object data)
  {
    removeComboItem(combo,data);
  }

  /** remove option menu items
   * @param combo option menu combo
   * @param data data of option menu items to remove
   */
  public static void removeOptionMenuItems(Combo combo, Object[] data)
  {
    removeComboItems(combo,data);
  }

  /** remove all option menu items
   * @param combo option menu combo
   */
  public static void removeAllOptionMenuItems(Combo combo)
  {
    removeAllComboItems(combo);
  }

  /** get option menu item
   * @param combo option menu combo
   * @param index item index
   * @return item
   */
  public static Object getOptionMenuItem(Combo combo, int index)
  {
    return getComboItem(combo,index);
  }

  /** get option menu items
   * @param combo option menu combo
   * @param items array
   * @return items array
   */
  public static <T> T[] getOptionMenuItems(Combo combo, T[] array)
  {
    return getComboItems(combo,array);
  }

  /** get option menu items
   * @param combo option menu combo
   * @param clazz data type class
   * @return items array
   */
  public static <T> T[] getOptionMenuItems(Combo combo, Class clazz)
  {
    return getComboItems(combo,clazz);
  }

  /** get selected option menu item
   * @param combo option menu combo
   * @param defaultValue default value
   * @return selected option menu item data
   */
  public static <T> T getSelectedOptionMenuItem(Combo combo, T defaultValue)
  {
    return getSelectedComboItem(combo,defaultValue);
  }

  /** set selected option menu item
   * @param combo option menu combo
   * @param data item data
   */
  public static <T> void setSelectedOptionMenuItem(Combo combo, T data)
  {
    setSelectedComboItem(combo,data);
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

    spinner = new Spinner(composite,style|SWT.RIGHT);
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
    tableColumn.setData(new TableLayoutData(0,0,0,0,0,0,0,width,0,((width == SWT.DEFAULT) || resizable) ? SWT.DEFAULT : width,((width == SWT.DEFAULT) || resizable) ? SWT.DEFAULT : width));
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
   * @param width width of column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, String title, int style, int width)
  {
    return addTableColumn(table,columnNb,title,style,width,(width == SWT.DEFAULT) ? true : false);
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
    return addTableColumn(table,columnNb,title,style,SWT.DEFAULT);
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
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  public static TableColumn addTableColumn(Table table, int columnNb, int style, boolean resizable)
  {
    return addTableColumn(table,columnNb,style,SWT.DEFAULT,resizable);
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
    return addTableColumn(table,columnNb,style,width,(width == SWT.DEFAULT) ? true : false);
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
   * @param showFlag true to show column, false for hide
   */
  public static void showTableColumn(TableColumn tableColumn, boolean showFlag)
  {
    if (showFlag)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)tableColumn.getData();

      if (tableLayoutData != null)
      {
        int width = Math.max(tableLayoutData.minWidth,16);
        tableColumn.setWidth(width);
        tableColumn.setResizable((width != SWT.DEFAULT) || (tableLayoutData.maxWidth != SWT.DEFAULT));
      }
      else
      {
        tableColumn.setWidth(60);
        tableColumn.setResizable(true);
      }
    }
    else
    {
      tableColumn.setWidth(0);
      tableColumn.setResizable(false);
    }
  }

  /** show table column
   * @param table table
   * @param columnNb column number
   * @param showFlag true to show column, false for hide
   */
  public static void showTableColumn(Table table, int columnNb, boolean showFlag)
  {
    TableColumn     tableColumn     = table.getColumn(columnNb);
    TableLayoutData tableLayoutData = (TableLayoutData)tableColumn.getData();
    if (showFlag)
    {
      if (tableLayoutData != null)
      {
        tableColumn.setWidth(tableLayoutData.minWidth);
        tableColumn.setResizable((tableLayoutData.minWidth != SWT.DEFAULT) || (tableLayoutData.maxWidth != SWT.DEFAULT));
      }
      else
      {
        tableColumn.setWidth(60);
      }
    }
    else
    {
      if (tableLayoutData != null) tableLayoutData.minWidth = tableColumn.getWidth();
      tableColumn.setWidth(0);
      tableColumn.setResizable(false);
    }
  }

  /** show table column
   * @param tableColumn table column to show
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
    for (int i = 0; i < tableColumns.length; i++)
    {
      width[i] = tableColumns[i].getWidth();
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
    for (int i = 0; i < Math.min(tableColumns.length,width.length); i++)
    {
      if (tableColumns[i].getResizable())
      {
        tableColumns[i].setWidth(width[i]);
      }
    }
  }

  /** set width of table columns
   * @param table table
   * @param width column width array
   */
  public static void adjustTableColumnWidth(Table table)
  {
    TableColumn[] tableColumns = table.getColumns();
    int[]         width        = new int[tableColumns.length];

    for (int i = 0; i < tableColumns.length; i++)
    {
      width[i] = 0;
    }

    GC gc = new GC(table);
    int margin = gc.textExtent("H").x;
    TableItem tableItems[] = table.getItems();
    for (int i = 0; i < tableColumns.length; i++)
    {
      for (TableItem tableItem : tableItems)
      {
        gc.setFont(tableItem.getFont(i));
        width[i] = Math.max(width[i],margin+gc.textExtent(tableItem.getText(i)).x+margin);
      }
    }
    gc.dispose();

    setTableColumnWidth(table,width);
  }

  /** get index of combo item
   * @param combo combo
   * @param data item data
   * @return index or -1
   */
  public static <T> int getTableIndex(final Table table, final T data)
  {
    final int index[] = new int[]{-1};

    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          TableItem tableItems[] = table.getItems();
          for (int i = 0; i < tableItems.length; i++)
          {
            if (data.equals(tableItems[i].getData()))
            {
              index[0] = i;
              break;
            }
          }
        }
      });
    }

    return index[0];
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

  /** swap table items
   * @param table table
   * @param tableItems table items
   * @param i,j indizes of table items to swap
   */
  private static void swapTableItems(Table table, TableItem tableItems[], int i, int j)
  {
    int columnCount = table.getColumnCount();

    // save data
    Object   data = tableItems[i].getData();
    String[] texts;
    if (columnCount > 0)
    {
      texts = new String[columnCount];
      for (int z = 0; z < columnCount; z++)
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
    TableItem tableItem = tableItems[i];

    // swap
    tableItems[i].setData(tableItems[j].getData());
    if (columnCount > 0)
    {
      for (int z = 0; z < columnCount; z++)
      {
        tableItems[i].setText(z,tableItems[j].getText(z));
      }
    }
    else
    {
      tableItems[i].setText(tableItems[j].getText());
    }
    tableItems[i].setForeground(tableItems[j].getForeground());
    tableItems[i].setBackground(tableItems[j].getBackground());
    tableItems[i].setChecked(tableItems[j].getChecked());

    tableItems[j].setData(data);
    if (columnCount > 0)
    {
      for (int z = 0; z < columnCount; z++)
      {
        tableItems[j].setText(z,texts[z]);
      }
    }
    else
    {
      tableItems[j].setText(texts[0]);
    }
    tableItems[j].setForeground(foregroundColor);
    tableItems[j].setBackground(backgroundColor);
    tableItems[j].setChecked(checked);
  }

  /** set sort column, toggle sort direction
   * @param table table
   * @param tableColumn table column to sort by
   */
  public static void setSortTableColumn(Table table, TableColumn tableColumn)
  {
    if (!table.isDisposed())
    {
      // set sort column, toggle sorting direction
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
    }
  }

  /** select sort column and sort table, toggle sort direction
   * @param table table
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  public static void sortTableColumn(Table table, TableColumn tableColumn, Comparator comparator)
  {
    if (!table.isDisposed())
    {
      // set sort column, toggle sorting direction
      setSortTableColumn(table,tableColumn);

      // sort table
      sortTableColumn(table,comparator);
    }
  }

  /** select sort column and sort table, toggle sort direction
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
      if ((table.getStyle() & SWT.VIRTUAL) == 0)
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
        int n = tableItems.length;
        int m;
        do
        {
          m = 1;
          for (int i = 0; i < n-1; i++)
          {
            boolean swapFlag = false;
            switch (sortDirection)
            {
              case SWT.UP:
                if (comparator != String.CASE_INSENSITIVE_ORDER)
                  swapFlag = (comparator.compare(tableItems[i].getData(),tableItems[i+1].getData()) > 0);
                else
                  swapFlag = (comparator.compare(tableItems[i].getText(sortColumnIndex),tableItems[i+1].getText(sortColumnIndex)) > 0);
                break;
              case SWT.DOWN:
                if (comparator != String.CASE_INSENSITIVE_ORDER)
                  swapFlag = (comparator.compare(tableItems[i].getData(),tableItems[i+1].getData()) < 0);
                else
                  swapFlag = (comparator.compare(tableItems[i].getText(sortColumnIndex),tableItems[i+1].getText(sortColumnIndex)) < 0);
                break;
            }
            if (swapFlag)
            {
              swapTableItems(table,tableItems,i,i+1);
              m = i+1;
            }
          }
          n = m;
//Dprintf.dprintf("--------------------------------------------------- %d %d",n,tableItems.length);
//for (int z = 0; z < n-1; z++) Dprintf.dprintf("%s: %d",tableItems[z].getData(),comparator.compare(tableItems[z].getData(),tableItems[z+1].getData()));
        }
        while (n > 1);
      }
      else
      {
        table.clearAll();
      }
    }
  }

  /** sort table column
   * @param table table
   * @param tableColumn table column
   * @param sortDirection sorting direction (SWT.UP, SWT.DOWN)
   */
  public static void sortTable(Table table, TableColumn tableColumn, int sortDirection)
  {
    if (!table.isDisposed())
    {
      table.setSortDirection(sortDirection);

      Event event = new Event();
      event.widget = tableColumn;
      tableColumn.notifyListeners(SWT.Selection,event);
    }
  }

  /** sort table column
   * @param table table
   * @param columnNb column index (0..n-1)
   * @param sortDirection sorting direction (SWT.UP, SWT.DOWN)
   */
  public static void sortTable(Table table, int columnNb, int sortDirection)
  {
    if (!table.isDisposed())
    {
      sortTable(table,table.getColumn(columnNb),sortDirection);
    }
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

      // get sort column index (default: first column)
      int sortColumnIndex = 0;
      TableColumn[] tableColumns = table.getColumns();
      for (int i = 0; i < tableColumns.length; i++)
      {
        if (table.getSortColumn() == tableColumns[i])
        {
          sortColumnIndex = i;
          break;
        }
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

  /** insert table item
   * @param table table
   * @param index insert before this index in table [0..n-1] or -1
   * @param table item data
   * @param values values list
   * @return insert index
   */
  public static TableItem insertTableItem(final Table table, final int index, final Object data, final Object... values)
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

  /** insert table item
   * @param table table
   * @param comparator table item comperator
   * @param table item data
   * @param values values list
   * @return table item
   */
  public static TableItem insertTableItem(final Table table, final Comparator comparator, final Object data, final Object... values)
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
              else
              {
                tableItem.setText(i,values[i].toString());
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

  /** add table item
   * @param table table
   * @param table item data
   * @param values values list
   * @return table item
   */
  public static TableItem addTableItem(Table table, Object data, Object... values)
  {
    return insertTableItem(table,-1,data,values);
  }

  /** update table item
   * @param tableItem table item to update
   * @param data item data
   * @param values values list
   */
  public static void updateTableItem(final TableItem tableItem, final Object data, final Object... values)
  {
    if (!tableItem.isDisposed())
    {
      tableItem.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tableItem.isDisposed())
          {
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
                else
                {
                  tableItem.setText(i,values[i].toString());
                }
              }
            }
          }
        }
      });
    }
  }

  /** update table item
   * @param table table
   * @param data item data
   * @param values values list
   * @return true if updated, false if not found
   */
  public static boolean updateTableItem(final Table table, final Object data, final Object... values)
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
            if (data.equals(tableItem.getData()))
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
                  else
                  {
                    tableItem.setText(i,values[i].toString());
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

  /** move table item
   * @param table table
   * @param data item data
   * @param offset move offset
   */
  public static void moveTableItem(final Table table, final int index, final int offset)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            // get table items
            TableItem[] tableItems = table.getItems();

            int i = index;
            int n = offset;

            // move item down
            while ((n > 0) && (i < tableItems.length-1))
            {
              swapTableItems(table,tableItems,i,i+1);
              i++;
              n--;
            }

            // move imte up
            while ((n < 0) && (i > 0))
            {
              swapTableItems(table,tableItems,i,i-1);
              i--;
              n++;
            }
          }
        }
      });
    }
  }

  /** move table item
   * @param table table
   * @param data item data
   * @param offset move offset
   */
  public static void moveTableItem(Table table, Object data, int offset)
  {
    // find index of item
    TableItem[] tableItems = table.getItems();
    int         i          = 0;
    while ((i < tableItems.length) && (tableItems[i].getData() != data))
    {
      i++;
    }

    if (i >= 0)
    {
      moveTableItem(table,i,offset);
    }
  }

  /** move table item
   * @param table table
   * @param data item data
   * @param offset move offset
   */
  public static void moveTableItem(Table table, TableItem tableItem, int offset)
  {
    moveTableItem(table,table.indexOf(tableItem),offset);
  }

  /** get table item
   * @param table table
   * @param data table item data
   * @return table item or null if not found
   */
  public static TableItem getTableItem(Table table, Object data)
  {
    for (TableItem tableItem : table.getItems())
    {
      if (data.equals(tableItem.getData()))
      {
        return tableItem;
      }
    }

    return null;
  }

  /** get table item
   * @param table table
   * @param data table item data
   * @return table item or null if not found
   */
  public static <T> TableItem getTableItem(Table table, Comparator<T> comparator, T data)
  {
    for (TableItem tableItem : table.getItems())
    {
      assert tableItem.getData() != null;
      if (comparator.compare((T)tableItem.getData(),data) == 0)
      {
        return tableItem;
      }
    }

    return null;
  }

  /** set table item color
   * @param table table
   * @param data table item data
   * @param foregroundColor foregound color
   * @param backgroundColor background color
   */
  public static void setTableItemColor(final Table table, final Object data, final Color foregroundColor, final Color backgroundColor)
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
              if (data.equals(tableItem.getData()))
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

  /** set table item color
   * @param table table
   * @param table item data
   * @param backgroundColor background color
   */
  public static void setTableItemColor(Table table, Object data, Color backgroundColor)
  {
    setTableItemColor(table,data,null,backgroundColor);
  }

  /** set table item color
   * @param table table
   * @param table item data
   * @param columnNb column (0..n-1)
   * @param foregroundColor foregound color
   * @param backgroundColor background color
   */
  public static void setTableItemColor(final Table table, final Object data, final int columnNb, final Color foregroundColor, final Color backgroundColor)
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
              if (data.equals(tableItem.getData()))
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

  /** set table item color
   * @param table table
   * @param table item data
   * @param columnNb column (0..n-1)
   * @param backgroundColor background color
   */
  public static void setTableItemColor(Table table, Object data, int columnNb, Color backgroundColor)
  {
    setTableItemColor(table,data,columnNb,null,backgroundColor);
  }

  /** set table item font
   * @param table table
   * @param table item data
   * @param font font
   */
  public static void setTableItemFont(final Table table, final Object data, final Font font)
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
              if (data.equals(tableItem.getData()))
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

  /** set table item font
   * @param table table
   * @param table item data
   * @param fontData font data
   */
  public static void setTableItemFont(final Table table, Object data, FontData fontData)
  {
    setTableItemFont(table,data,new Font(table.getDisplay(),fontData));
  }

  /** set table item font
   * @param table table
   * @param table item data
   * @param columnNb column (0..n-1)
   * @param font font
   */
  public static void setTableItemFont(final Table table, final Object data, final int columnNb, final Font font)
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
              if (data.equals(tableItem.getData()))
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

  /** set table item font
   * @param table table
   * @param table item data
   * @param columnNb column (0..n-1)
   * @param fontData font data
   */
  public static void setTableItemFont(final Table table, Object data, int columnNb, FontData fontData)
  {
    setTableItemFont(table,data,columnNb,new Font(table.getDisplay(),fontData));
  }

  /** set table item checked
   * @param table table
   * @param table item data
   * @param checked checked flag
   */
  public static void setTableItemChecked(final Table table, final Object data, final boolean checked)
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
              if (data.equals(tableItem.getData()))
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

  /** remove table item
   * @param table table
   * @param data item data
   */
  public static void removeTableItem(final Table table, final Object data)
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
              if (data.equals(tableItem.getData()))
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

  /** remove table item
   * @param table table
   * @param tableItem table item to remove
   */
  public static void removeTableItem(final Table table, final TableItem tableItem)
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
      removeTableItem(table,tableItem);
    }
  }

  /** remove all table items
   * @param table table
   */
  public static void removeAllTableItems(final Table table)
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

  /** refresh virtual table
   * @param table table
   */
  public static void refreshVirtualTable(final Table table)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          table.setRedraw(false);

          int count    = table.getItemCount();
          int topIndex = table.getTopIndex();

          table.setItemCount(0);
          table.clearAll();

          table.setItemCount(count);
          table.setTopIndex(topIndex);

          table.setRedraw(true);
        }
      });
    }
  }

  /** get table items data
   * @param table table
   * @param entries array
   * @return items data array
   */
  public static <T> T[] getTableItems(Table table, T[] array)
  {
    TableItem tableItems[] = table.getItems();

    if (array.length != tableItems.length)
    {
      array = Arrays.copyOf(array,tableItems.length);
    }

    for (int i = 0; i < tableItems.length; i++)
    {
      array[i] = (T)tableItems[i].getData();
    }

    return array;
  }

  /** get table items data
   * @param table table
   * @param clazz class of array elements
   * @return items data array
   */
  public static <T> T[] getTableItems(Table table, Class clazz)
  {
    TableItem tableItems[] = table.getItems();

    T[] array = (T[])Array.newInstance(clazz,tableItems.length);
    for (int i = 0; i < tableItems.length; i++)
    {
      array[i] = (T)tableItems[i].getData();
    }

    return array;
  }

  /** get selected table item
   * @param table table
   * @return selected table item data or null
   */
  public static <T> T getSelectedTableItem(final Table table)
  {
    final Object data[] = new Object[1];

    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            TableItem tableItems[] = table.getSelection();
            if (tableItems.length > 0)
            {
              data[0] = tableItems[0];
            }
            else
            {
              data[0] = null;
            }
          }
        }
      });
    }

    return (T)data[0];
  }

  /** clear selected table item
   * @param table table
   */
  public static <T> void clearSelectedTableItem(final Table table)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          table.deselectAll();
        }
      });
    }
  }

  /** set selected table item
   * @param table table
   * @param data item data
   */
  public static <T> void setSelectedTableItem(final Table table, final T data)
  {
    if (!table.isDisposed())
    {
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!table.isDisposed())
          {
            table.deselectAll();
            TableItem tableItems[] = table.getItems();
            for (TableItem tableItem : tableItems)
            {
              if (data.equals(tableItem.getData()))
              {
                table.setSelection(tableItem);
                break;
              }
            }
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
    treeColumn.setData(new TableLayoutData(0,0,0,0,0,0,0,width,0,((width == SWT.DEFAULT) || resizable) ? SWT.DEFAULT : width,((width == SWT.DEFAULT) || resizable) ? SWT.DEFAULT : width));
    treeColumn.setWidth(width);
    treeColumn.setResizable(resizable);
    if (width <= 0) treeColumn.pack();

    return treeColumn;
  }

  /** show tree column
   * @param treeColumn tree column to show
   * @param width tree column width
   * @param showFlag true to show column, false for hide
   */
  public static void showTreeColumn(TreeColumn treeColumn, boolean showFlag)
  {
    if (showFlag)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)treeColumn.getData();

      if (tableLayoutData != null)
      {
        treeColumn.setWidth(tableLayoutData.minWidth);
        treeColumn.setResizable((tableLayoutData.minWidth != SWT.DEFAULT) || (tableLayoutData.maxWidth != SWT.DEFAULT));
      }
      else
      {
        treeColumn.setWidth(60);
      }
    }
    else
    {
      treeColumn.setWidth(0);
      treeColumn.setResizable(false);
    }
  }

  /** show tree column
   * @param tree tree
   * @param columnNb column number
   * @param showFlag true to show column, false for hide
   */
  public static void showTreeColumn(Tree tree, int columnNb, boolean showFlag)
  {
    TreeColumn      treeColumn      = tree.getColumn(columnNb);
    TableLayoutData tableLayoutData = (TableLayoutData)treeColumn.getData();
    if (showFlag)
    {
      if (tableLayoutData != null)
      {
        int width = Math.max(tableLayoutData.minWidth,16);
        treeColumn.setWidth(width);
        treeColumn.setResizable((width != SWT.DEFAULT) || (tableLayoutData.maxWidth != SWT.DEFAULT));
      }
      else
      {
        treeColumn.setWidth(60);
        treeColumn.setResizable(true);
      }
    }
    else
    {
      if (tableLayoutData != null) tableLayoutData.minWidth = treeColumn.getWidth();
      tableLayoutData.minWidth = treeColumn.getWidth();
      treeColumn.setWidth(0);
      treeColumn.setResizable(false);
    }
  }

  /** show tree column
   * @param treeColumn tree column to show
   */
  public static void showTreeColumn(TreeColumn treeColumn)
  {
    showTreeColumn(treeColumn,true);
  }

  /** hide tree column
   * @param treeColumn tree column to hide
   */
  public static void hideTreeColumn(TreeColumn treeColumn)
  {
    showTreeColumn(treeColumn,false);
  }

  /** get insert position in sorted tree
   * @param tree tree
   * @param comparator table data comparator
   * @param data data
   * @return index in tree
   */
  public static int getTreeItemIndex(Tree tree, Comparator comparator, Object data)
  {
    int index = 0;

    if (!tree.isDisposed())
    {
      TreeItem[] treeItems = tree.getItems();

      // get sort column index (default: first column)
      int sortColumnIndex = 0;
      TreeColumn[] treeColumns = tree.getColumns();
      for (int i = 0; i < treeColumns.length; i++)
      {
        if (tree.getSortColumn() == treeColumns[i])
        {
          sortColumnIndex = i;
          break;
        }
      }

      // get sorting direction
      int sortDirection = tree.getSortDirection();
      if (sortDirection == SWT.NONE) sortDirection = SWT.UP;

      // find insert index
      boolean foundFlag = false;
      while ((index < treeItems.length) && !foundFlag)
      {
        switch (sortDirection)
        {
          case SWT.UP:
            if (comparator != String.CASE_INSENSITIVE_ORDER)
              foundFlag = (comparator.compare(treeItems[index].getData(),data) > 0);
            else
              foundFlag = (comparator.compare(treeItems[index].getText(sortColumnIndex),data) > 0);
            break;
          case SWT.DOWN:
            if (comparator != String.CASE_INSENSITIVE_ORDER)
              foundFlag = (comparator.compare(treeItems[index].getData(),data) < 0);
            else
              foundFlag = (comparator.compare(treeItems[index].getText(sortColumnIndex),data) < 0);
            break;
        }
        if (!foundFlag) index++;
      }
    }

    return index;
  }

  /** insert tree item at index
   * @param tree tree widget
   * @param index index (0..n)
   * @param data data
   * @param image image
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem insertTreeItem(final Tree tree, final int index, final Object data, final Image image, final boolean folderFlag, final Object... values)
  {
    /** tree insert runnable
     */
    class TreeRunnable implements Runnable
    {
      TreeItem treeItem = null;

      public void run()
      {
        if (!tree.isDisposed())
        {
          if (index >= 0)
          {
            treeItem = new TreeItem(tree,SWT.CHECK,index);
          }
          else
          {
            treeItem = new TreeItem(tree,SWT.CHECK);
          }
          treeItem.setData(data);
          treeItem.setImage(image);
          if (folderFlag) new TreeItem(treeItem,SWT.NONE);
          for (int i = 0; i < values.length; i++)
          {
            if (values[i] != null)
            {
              if      (values[i] instanceof String)
              {
                treeItem.setText(i,(String)values[i]);
              }
              else if (values[i] instanceof Image)
              {
                treeItem.setImage(i,(Image)values[i]);
              }
              else
              {
                treeItem.setText(i,values[i].toString());
              }
            }
          }
        }
      }
    };

    TreeRunnable treeRunnable = new TreeRunnable();
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(treeRunnable);
    }

    return treeRunnable.treeItem;
  }

  /** insert tree item
   * @param tree tree widget
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem insertTreeItem(final Tree tree, int index, Object data, boolean folderFlag, Object... values)
  {
    return insertTreeItem(tree,index,data,(Image)null,folderFlag,values);
  }

  /** add tree item at end
   * @param tree tree widget
   * @param data data
   * @param image image
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(Tree tree, Object data, Image image, boolean folderFlag, Object... values)
  {
    return insertTreeItem(tree,-1,data,image,folderFlag,values);
  }

  /** add tree item at end
   * @param tree tree widget
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  public static TreeItem addTreeItem(Tree tree, Object data, boolean folderFlag, Object... values)
  {
    return insertTreeItem(tree,-1,data,folderFlag,values);
  }

  /** insert sub-tree item
   * @param parentTreeItem parent tree item
   * @param index index (0..n)
   * @param data data
   * @param image image
   * @param folderFlag TRUE iff foler
   * @param values values list
   * @return new tree item
   */
  public static TreeItem insertTreeItem(final TreeItem parentTreeItem, final int index, final Object data, final Image image, final boolean folderFlag, final Object... values)
  {
    /** tree insert runnable
     */
    class TreeRunnable implements Runnable
    {
      TreeItem treeItem = null;

      public void run()
      {
        if (!parentTreeItem.isDisposed())
        {
          if (index >= 0)
          {
            treeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
          }
          else
          {
            treeItem = new TreeItem(parentTreeItem,SWT.NONE);
          }
          treeItem.setData(data);
          treeItem.setImage(image);
          if (folderFlag) new TreeItem(treeItem,SWT.NONE);
          for (int i = 0; i < values.length; i++)
          {
            if (values[i] != null)
            {
              if      (values[i] instanceof String)
              {
                treeItem.setText(i,(String)values[i]);
              }
              else if (values[i] instanceof Image)
              {
                treeItem.setImage(i,(Image)values[i]);
              }
              else
              {
                treeItem.setText(i,values[i].toString());
              }
            }
          }
        }
      }
    };

    TreeRunnable treeRunnable = new TreeRunnable();
    if (!parentTreeItem.isDisposed())
    {
      parentTreeItem.getDisplay().syncExec(treeRunnable);
    }

    return treeRunnable.treeItem;
  }

  /** insert sub-tree item
   * @param parentTreeItem parent tree item
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @param values values list
   * @return new tree item
   */
  public static TreeItem insertTreeItem(final TreeItem parentTreeItem, int index, Object data, boolean folderFlag, Object... values)
  {
    return insertTreeItem(parentTreeItem,index,data,null,folderFlag,values);
  }

  /** add sub-tree item at end
   * @param parentTreeItem parent tree item
   * @param data data
   * @param image image
   * @param folderFlag TRUE iff foler
   * @param values values list
   * @return new tree item
   */
  public static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, Image image, boolean folderFlag, Object... values)
  {
    return insertTreeItem(parentTreeItem,-1,data,image,folderFlag,values);
  }

  /** add sub-tree item at end
   * @param parentTreeItem parent tree item
   * @param data data
   * @param folderFlag TRUE iff foler
   * @param values values list
   * @return new tree item
   */
  public static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, boolean folderFlag, Object... values)
  {
    return addTreeItem(parentTreeItem,data,null,folderFlag,values);
  }

  /** add sub-tree item at end
   * @param parentTreeItem parent tree item
   * @param data data
   * @param values values list
   * @return new tree item
   */
  public static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, Object... values)
  {
    return addTreeItem(parentTreeItem,data,false,values);
  }

  /** get tree item from sub-tree items
   * @param parentTreeItem parent tree item
   * @param data tree item data
   * @return tree item or null if not found
   */
  public static <T extends Comparable> TreeItem getTreeItem(TreeItem parentTreeItem, T data)
  {
    for (TreeItem treeItem : parentTreeItem.getItems())
    {
      if (data.compareTo(treeItem.getData()) == 0)
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        treeItem = getTreeItem(treeItem,data);
        if (treeItem != null)
        {
          return treeItem;
        }
      }
    }

    return null;
  }


  /** get tree item from sub-tree items
   * @param parentTreeItem parent tree item
   * @param data tree item data
   * @return tree item or null if not found
   */
  public static TreeItem getTreeItem(TreeItem parentTreeItem, Object data)
  {
    for (TreeItem treeItem : parentTreeItem.getItems())
    {
      if (data.equals(treeItem.getData()))
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        treeItem = getTreeItem(treeItem,data);
        if (treeItem != null)
        {
          return treeItem;
        }
      }
    }

    return null;
  }

  /** get tree item from sub-tree items
   * @param parentTreeItem parent tree item
   * @param data tree item data
   * @return tree item or null if not found
   */
  public static <T> TreeItem getTreeItem(TreeItem parentTreeItem, Comparator<T> comparator, T data)
  {
    for (TreeItem treeItem : parentTreeItem.getItems())
    {
      assert treeItem.getData() != null;
      if (comparator.compare((T)treeItem.getData(),data) == 0)
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        treeItem = getTreeItem(treeItem,comparator,data);
        if (treeItem != null)
        {
          return treeItem;
        }
      }
    }

    return null;
  }

  /** get tree item
   * @param tree tree
   * @param data tree item data
   * @return tree item or null if not found
   */
  public static <T extends Comparable> TreeItem getTreeItem(Tree tree, T data)
  {
    TreeItem subTreeItem;

    for (TreeItem treeItem : tree.getItems())
    {
      if (data.compareTo(treeItem.getData()) == 0)
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        subTreeItem = getTreeItem(treeItem,data);
        if (subTreeItem != null)
        {
          return subTreeItem;
        }
      }
    }

    return null;
  }

  /** get tree item
   * @param tree tree
   * @param data tree item data
   * @return tree item or null if not found
   */
  public static TreeItem getTreeItem(Tree tree, Object data)
  {
    TreeItem subTreeItem;

    for (TreeItem treeItem : tree.getItems())
    {
      if (data.equals(treeItem.getData()))
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        subTreeItem = getTreeItem(treeItem,data);
        if (subTreeItem != null)
        {
          return subTreeItem;
        }
      }
    }

    return null;
  }

  /** get tree item
   * @param tree tree
   * @param comparator tree item comparator
   * @return tree item or null if not found
   */
  public static <T> TreeItem getTreeItem(Tree tree, Comparator<T> comparator, T data)
  {
    TreeItem subTreeItem;

    for (TreeItem treeItem : tree.getItems())
    {
      assert treeItem.getData() != null;
      if (comparator.compare((T)treeItem.getData(),data) == 0)
      {
        return treeItem;
      }
      if (treeItem.getExpanded())
      {
        subTreeItem = getTreeItem(treeItem,comparator);
        if (subTreeItem != null)
        {
          return subTreeItem;
        }
      }
    }

    return null;
  }

  /** set table item checked
   * @param table table
   * @param table item data
   * @param checked checked flag
   */
  public static void setTreeItemChecked(final Tree tree, final Object data, final boolean checked)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            for (TreeItem treeItem : tree.getItems())
            {
              if (data.equals(treeItem.getData()))
              {
                treeItem.setChecked(checked);
                break;
              }
            }
          }
        }
      });
    }
  }

  /** update tree item
   * @param treeItem tree item to update
   * @param data item data
   * @param values values list
   */
  public static void updateTreeItem(final TreeItem treeItem, final Object data, final Object... values)
  {
    if (!treeItem.isDisposed())
    {
      treeItem.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!treeItem.isDisposed())
          {
            treeItem.setData(data);
            for (int i = 0; i < values.length; i++)
            {
              if (values[i] != null)
              {
                if      (values[i] instanceof String)
                {
                  treeItem.setText(i,(String)values[i]);
                }
                else if (values[i] instanceof Image)
                {
                  treeItem.setImage(i,(Image)values[i]);
                }
                else
                {
                  treeItem.setText(i,values[i].toString());
                }
              }
            }
          }
        }
      });
    }
  }

  /** update tree item
   * @param tree tree
   * @param data item data
   * @param values values list
   * @return true if updated, false if not found
   */
  public static boolean updateTreeItem(final Tree tree, final Object data, final Object... values)
  {
    /** table update runnable
     */
    class TreeRunnable implements Runnable
    {
      boolean updatedFlag = false;

      public void run()
      {
        if (!tree.isDisposed())
        {
          for (TreeItem treeItem : tree.getItems())
          {
            if (data.equals(treeItem.getData()))
            {
              for (int i = 0; i < values.length; i++)
              {
                if (values[i] != null)
                {
                  if      (values[i] instanceof String)
                  {
                    treeItem.setText(i,(String)values[i]);
                  }
                  else if (values[i] instanceof Image)
                  {
                    treeItem.setImage(i,(Image)values[i]);
                  }
                  else
                  {
                    treeItem.setText(i,values[i].toString());
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

    TreeRunnable treeRunnable = new TreeRunnable();
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(treeRunnable);
    }

    return treeRunnable.updatedFlag;
  }

  /** remove tree item
   * @param treeItem tree item
   * @param data item data
   * @param true iff tree item removed
   */
  private static boolean removeTreeItem(TreeItem treeItem, Object data)
  {
    if (data.equals(treeItem.getData()))
    {
      treeItem.dispose();
      return true;
    }
    else
    {
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        if (removeTreeItem(subTreeItem,data))
        {
          if (treeItem.getItemCount() <= 0)
          {
            treeItem.setExpanded(false);
            new TreeItem(treeItem,SWT.NONE);
          }
          return true;
        }
      }
      return false;
    }
  }

  /** remove tree item
   * @param tree tree
   * @param data item data
   */
  public static void removeTreeItem(final Tree tree, final Object data)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            for (TreeItem treeItem : tree.getItems())
            {
              if (removeTreeItem(treeItem,data))
              {
                break;
              }
            }
          }
        }
      });
    }
  }

  /** remove tree item
   * @param tree tree
   * @param treeItem tree item to remove
   */
  public static void removeTreeItem(final Tree tree, final TreeItem treeItem)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            TreeItem parentTreeItem = treeItem.getParentItem();
            treeItem.dispose();
            if ((parentTreeItem != null) && (parentTreeItem.getItemCount() <= 0))
            {
              parentTreeItem.setExpanded(false);
              new TreeItem(parentTreeItem,SWT.NONE);
            }
          }
        }
      });
    }
  }

  /** remove tree items
   * @param tree tree
   * @param treeItems tree items to remove
   */
  public static void removeTreeItems(Tree tree, TreeItem[] treeItems)
  {
    for (TreeItem treeItem : treeItems)
    {
      removeTreeItem(tree,treeItem);
    }
  }

  /** remove all tree items of tree item
   * @param tree tree
   * @treeItem tree item
   */
  public static void removeAllTreeItems(final Tree tree, final TreeItem treeItem)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            treeItem.removeAll();
            new TreeItem(treeItem,SWT.NONE);
            treeItem.setExpanded(false);
          }
        }
      });
    }
  }

  /** remove all tree items
   * @param tree tree
   */
  public static void removeAllTreeItems(final Tree tree)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            tree.removeAll();
          }
        }
      });
    }
  }

/*
private void printItems(int i, TreeItem treeItem)
{
Dprintf.dprintf("%s%s: %s",StringUtils.repeat("  ",i),treeItem,treeItem.getData());
for (TreeItem subTreeItem : treeItem.getItems()) printItems(i+1,subTreeItem);
}
private void printTree(Tree tree)
{
Dprintf.dprintf("--- tree:");
for (TreeItem treeItem : tree.getItems()) printItems(0,treeItem);
Dprintf.dprintf("---");
}

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
      for (TreeItem subTreeItem : subTreeItems)
      {
        sortSubTreeColumn(tree,subTreeItem,subTreeItem.getItems(),sortDirection,comparator);
      }

      // sort sub-tree
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
      if ((tree.getStyle() & SWT.VIRTUAL) == 0)
      {
        // sort sub-trees
        for (TreeItem subTreeItem : subTreeItems)
        {
          sortSubTreeColumn(tree,subTreeItem,subTreeItem.getItems(),sortDirection,comparator);
        }

        // sort tree
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
      }
      else
      {
        tree.clearAll(true);
      }
    }
  }

  /** get expanded (open) tree items in tree
   * @param treeItemSet hash-set for expanded tree items
   * @param treeItem tree item to start
   * @param rootItemsOnly true to collect expanded sub-tree root items only
   */
  private static void getSubTreeItems(HashSet<TreeItem> treeItemSet, TreeItem treeItem)
  {
    for (TreeItem subTreeItem : treeItem.getItems())
    {
      treeItemSet.add(subTreeItem);
      if (subTreeItem.getExpanded())
      {
        getSubTreeItems(treeItemSet,subTreeItem);
      }
    }
  }

  /** get all tree items in tree
   * @param tree tree
   * @return tree items array
   */
  public static TreeItem[] getAllTreeItems(Tree tree)
  {
    HashSet<TreeItem> treeItemSet = new HashSet<TreeItem>();
    if (!tree.isDisposed())
    {
      for (TreeItem treeItem : tree.getItems())
      {
        treeItemSet.add(treeItem);
        getSubTreeItems(treeItemSet,treeItem);
      }
    }

    return treeItemSet.toArray(new TreeItem[treeItemSet.size()]);
  }

  /** get all sub tree items of tree item
   * @param treeItem tree item
   * @return tree items array
   */
  public static TreeItem[] getAllTreeItems(TreeItem treeItem)
  {
    HashSet<TreeItem> treeItemSet = new HashSet<TreeItem>();
    if (!treeItem.isDisposed())
    {
      getSubTreeItems(treeItemSet,treeItem);
    }

    return treeItemSet.toArray(new TreeItem[treeItemSet.size()]);
  }

  /** re-expand entries
   * @param expandedItemSet data entries to re-expand
   * @return treeItem tree item to start
   */
  private static void reExpandTreeItems(HashSet expandedItemSet, TreeItem treeItem)
  {
    if (!treeItem.isDisposed())
    {
      treeItem.setExpanded(expandedItemSet.contains(treeItem.getData()));
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        reExpandTreeItems(expandedItemSet,subTreeItem);
      }
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

  /** set sort tree column
   * @param tree tree
   * @param treeColumn tree column to sort by
   */
  public static void setSortTreeColumn(Tree tree, TreeColumn treeColumn, int sortDirection)
  {
    if (!tree.isDisposed())
    {
      tree.setSortColumn(treeColumn);
      tree.setSortDirection(sortDirection);
    }
  }

  /** set sort tree column
   * @param tree tree
   * @param treeColumn tree column to sort by
   */
  public static void setSortTreeColumn(Tree tree, int columnNb, int sortDirection)
  {
    if (!tree.isDisposed())
    {
      setSortTreeColumn(tree,tree.getColumn(columnNb),sortDirection);
    }
  }

  /** set sort tree column, toggle sort direction
   * @param tree tree
   * @param treeColumn tree column to sort by
   */
  public static void setSortTreeColumn(Tree tree, TreeColumn treeColumn)
  {
    if (!tree.isDisposed())
    {
      // get/toggle sorting direction
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

      // set column sort indicators
      setSortTreeColumn(tree,treeColumn,sortDirection);
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
      HashSet expandedItemSet = new HashSet();
      for (TreeItem treeItem : tree.getItems())
      {
        getExpandedTreeData(expandedItemSet,treeItem);
      }

      // sort column
//printTree(tree);
      sortTreeColumn(tree,tree.getItems(),sortDirection,comparator);

      // restore expanded sub-trees
      for (TreeItem treeItem : tree.getItems())
      {
        reExpandTreeItems(expandedItemSet,treeItem);
      }

      // set column sort indicators
      tree.setSortColumn(treeColumn);
      tree.setSortDirection(sortDirection);
    }
  }

  /** select sort column and sort tree, toggle sort direction
   * @param tree tree
   * @param columnNb column index to sort by (0..n-1)
   * @param comparator tree data comparator
   */
  public static void sortTreeColumn(Tree tree, int columnNb, Comparator comparator)
  {
    sortTreeColumn(tree,tree.getColumn(columnNb),comparator);
  }

  /** set sort tree column, toggle sort direction
   * @param tree tree
   * @param treeColumn table column to sort by
   * @param sortDirection sort direction (SWT.UP, SWT.DOWN)
   */
  public static void sortTreeColumn(Tree tree, Comparator comparator)
  {
    if (!tree.isDisposed())
    {
Dprintf.dprintf("");
    }
  }

  /** sort tree column
   * @param tree tree
   * @param treeColumn table column to sort by
   * @param sortDirection sort direction (SWT.UP, SWT.DOWN)
   */
  public static void sortTree(Tree tree, TreeColumn treeColumn, int sortDirection)
  {
    if (!tree.isDisposed())
    {
      tree.setSortDirection(sortDirection);

      Event event = new Event();
      event.widget = treeColumn;
      treeColumn.notifyListeners(SWT.Selection,event);
    }
  }

  /** sort tree column
   * @param tree tree
   * @param treeColumn tree column number to sort by [0..n-1]
   * @param sortDirection sort direction (SWT.UP, SWT.DOWN)
   */
  public static void sortTree(Tree tree, int columnNb, int sortDirection)
  {
    if (!tree.isDisposed())
    {
      sortTree(tree,tree.getColumn(columnNb),sortDirection);
    }
  }

  /** sort tree (ascending)
   * @param table table
   * @param columnNb column index (0..n-1)
   */
  public static void sortTree(Tree tree, int columnNb)
  {
    sortTree(tree,columnNb,SWT.UP);
  }


  /** get width of tree columns
   * @param tree tree
   * @return tree columns width array
   */
  public static int[] getTreeColumnWidth(Tree tree)
  {
    TreeColumn[] treeColumns = tree.getColumns();
    int[] width = new int[treeColumns.length];
    for (int i = 0; i < treeColumns.length; i++)
    {
      width[i] = treeColumns[i].getWidth();
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
    for (int i = 0; i < Math.min(treeColumns.length,width.length); i++)
    {
      if (treeColumns[i].getResizable())
      {
        treeColumns[i].setWidth(width[i]);
      }
    }
  }

  /** set width of tree columns
   * @param tree tree
   * @param index column index (0..n-1)
   * @param width column width array
   */
  public static void setTreeColumnWidth(Tree tree, int index, int width)
  {
    TreeColumn[] treeColumns = tree.getColumns();
    treeColumns[index].setWidth(width);
  }

  /** get selected tree item
   * @param tree tree
   * @param default default value
   * @return selected tree item data
   */
  public static <T> T getSelectedTreeItem(final Tree tree, T defaultValue)
  {
    final Object data[] = new Object[]{defaultValue};

    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            TreeItem treeItems[] = tree.getSelection();
            if (treeItems.length > 0)
            {
              data[0] = treeItems[0].getData();
            }
          }
        }
      });
    }

    return (T)data[0];
  }

  /** clearSelected tree item
   * @param tree table
   */
  public static <T> void clearSelectedTreeItem(final Tree tree)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            tree.deselectAll();
          }
        }
      });
    }
  }

  /** set selected tree item
   * @param tree table
   * @param data item data
   */
  public static <T> void setSelectedTreeItem(final Tree tree, final T data)
  {
    if (!tree.isDisposed())
    {
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            tree.deselectAll();
            TreeItem treeItems[] = tree.getItems();
            for (TreeItem treeItem : treeItems)
            {
              if (data.equals(treeItem.getData()))
              {
                tree.setSelection(treeItem);
                break;
              }
            }
          }
        }
      });
    }
  }

  //-----------------------------------------------------------------------

  /** create new sash widget (pane)
   * @param composite composite widget
   * @param style style
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
   * @param count number of pane sections
   * @param style style
   * @return new pane widget
   */
  public static Pane newPane(Composite composite, int count, int style)
  {
    Pane pane = new Pane(composite,count,style);

    return pane;
  }

  /** create new pane widget
   * @param composite composite widget
   * @param count number of pane sections
   * @return new pane widget
   */
  public static Pane newPane(Composite composite, int count)
  {
    return newPane(composite,count,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** create new tab folder
   * @param compositet composite
   * @param style style
   * @return new tab folder widget
   */
  public static TabFolder newTabFolder(Composite composite, int style)
  {
    final Image IMAGE_CLOSE = new Image(Display.getDefault(),IMAGE_CLOSE_DATA);

    TabFolder tabFolder = new TabFolder(composite,style|SWT.TOP|SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));

    tabFolder.addMouseListener(new MouseListener()
    {
      public void mouseDoubleClick(MouseEvent mouseEvent)
      {
      }
      public void mouseDown(final MouseEvent mouseEvent)
      {
      }
      public void mouseUp(final MouseEvent mouseEvent)
      {
        TabFolder tabFolder          = (TabFolder)mouseEvent.widget;
        TabItem   selectedTabItems[] = tabFolder.getSelection();
        if ((selectedTabItems != null) && (selectedTabItems.length > 0))
        {
          if (selectedTabItems[0].getImage() != null)
          {
            Rectangle bounds = selectedTabItems[0].getBounds();
            if ((mouseEvent.x > bounds.x) && (mouseEvent.x < bounds.x+IMAGE_CLOSE.getBounds().width))
            {
              Event event = new Event();
              event.item = selectedTabItems[0];
              tabFolder.notifyListeners(SWT.Close,event);
            }
          }
        }
      }
    });

    return tabFolder;
  }

  /** create new tab folder
   * @param compositet composite
   * @return new tab folder widget
   */
  public static TabFolder newTabFolder(Composite composite)
  {
    return newTabFolder(composite,SWT.NONE);
  }

  /** insert tab widget
   * @param tabFolder tab folder
   * @param leftComposite left tab item composite or null
   * @param title title of tab
   * @param titleImage title image of tab
   * @param data data element
   * @param style style
   * @param isVisible true for visible, false otherwise
   * @return new composite widget
   */
  public static Composite insertTab(TabFolder tabFolder, Composite leftComposite, String title, Object data, int style, boolean isVisible)
  {
    final Image IMAGE_CLOSE = new Image(Display.getDefault(),IMAGE_CLOSE_DATA);

    // get tab item index
    int index = 0;
    TabItem[] tabItems = tabFolder.getItems();
    while(index < tabItems.length)
    {
      if (tabItems[index].getControl() == leftComposite)
      {
        index++;
        break;
      }
      index++;
    }

    // create tab
/*
    if ((style & SWT.CLOSE) == SWT.CLOSE)
    {
      titleImage = getCloseImage();
    }
    TabItem tabItem = new TabItem(tabFolder,SWT.NONE,index);
    tabItem.setData(data);
    if (titleImage != null)
    {
      Point     titleSize        = Widgets.getTextSize(tabFolder,title);
      Rectangle titleImageBounds = titleImage.getBounds();
      int       w                = titleSize.x + 4 + titleImageBounds.width;
      int       h                = Math.max(titleSize.y,titleImageBounds.height);

      Image image = new Image(tabFolder.getDisplay(),w,h);
      GC gc = new GC(image);
      gc.setForeground(tabFolder.getForeground());
      gc.setBackground(tabFolder.getBackground());
      gc.fillRectangle(0,0,w,h);
      gc.drawText(title,0,0,true);
      gc.drawImage(titleImage,titleSize.x + 4,(titleSize.y - titleImageBounds.height) / 2);
      gc.dispose();
    }
    else
    {
      tabItem.setText(title);
    }
*/
Composite composite;
if (isVisible) {
    TabItem tabItem = new TabItem(tabFolder,SWT.NONE,index);
    tabItem.setData(data);
    tabItem.setText(title);
    if ((style & SWT.CLOSE) == SWT.CLOSE)
    {
      tabItem.setImage(IMAGE_CLOSE);
    }

    // create composite
    composite = new Composite(tabFolder,SWT.BORDER|SWT.NONE);
    composite.setLayoutData(new TableLayoutData(isVisible));
    composite.setLayout(new TableLayout(1.0,1.0,2));

    tabItem.setControl(composite);
}
else
{
    composite = new Composite(tabFolder,SWT.BORDER|SWT.NONE);
    composite.setLayoutData(new TableLayoutData(isVisible));
    composite.setLayout(new TableLayout(1.0,1.0,2));
    composite.setVisible(false);
}

    return composite;
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @param data data element
   * @param style style
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, Object data, int style, boolean isVisible)
  {
    return insertTab(tabFolder,null,title,data,style,isVisible);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @param data data element
   * @param style style
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, Object data, int style)
  {
    return addTab(tabFolder,title,data,style,true);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @param data data element
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, Object data, boolean isVisible)
  {
    return addTab(tabFolder,title,data,SWT.NONE,isVisible);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, int style, boolean isVisible)
  {
    return addTab(tabFolder,title,null,style,isVisible);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title, boolean isVisible)
  {
    return addTab(tabFolder,title,SWT.NONE,isVisible);
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  public static Composite addTab(TabFolder tabFolder, String title)
  {
    return addTab(tabFolder,title,SWT.NONE,true);
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
    for (int i = 0; i < tabItems.length; i++)
    {
      tabList[i] = (Composite)tabItems[i].getControl();
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

  /** create new c-tab folder
   * @param compositet composite
   * @return new tab folder widget
   */
  public static CTabFolder newCTabFolder(Composite composite, int style)
  {
    CTabFolder cTabFolder = new CTabFolder(composite,style|SWT.TOP|SWT.FLAT);
    cTabFolder.setSimple(true);
    cTabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));

    return cTabFolder;
  }

  /** create new c-tab folder
   * @param compositet composite
   * @return new tab folder widget
   */
  public static CTabFolder newCTabFolder(Composite composite)
  {
    return newCTabFolder(composite,SWT.NONE);
  }

  /** insert tab widget
   * @param cTabFolder c-tab folder
   * @param leftComposite left tab item composite or null
   * @param title title of tab
   * @param data data element
   * @return new composite widget
   */
  public static Composite insertTab(CTabFolder cTabFolder, Composite leftComposite, String title, Object data, boolean isVisible)
  {
    // get tab item index
    int index = 0;
    CTabItem[] cTabItems = cTabFolder.getItems();
    for (index = 0; index < cTabItems.length; index++)
    {
      if (cTabItems[index].getControl() == leftComposite)
      {
        index++;
        break;
      }
    }

    // create tab
    CTabItem cTabItem = new CTabItem(cTabFolder,SWT.NONE,index);
    cTabItem.setData(data);
    cTabItem.setText(title);

    // create composite
    Composite composite = new Composite(cTabFolder,SWT.BORDER|SWT.NONE);
    TableLayout tableLayout = new TableLayout();
    tableLayout.marginTop    = 20;
    tableLayout.marginBottom = 20;
    tableLayout.marginLeft   = 20;
    tableLayout.marginRight  = 20;
    composite.setLayout(tableLayout);

    cTabItem.setControl(composite);

    return composite;
  }

  /** add tab widget
   * @param cTabFolder c-tab folder
   * @param title title of tab
   * @param data data element
   * @return new composite widget
   */
  public static Composite addTab(CTabFolder cTabFolder, String title, Object data, boolean isVisible)
  {
    return insertTab(cTabFolder,null,title,data,isVisible);
  }

  /** add tab widget
   * @param cTabFolder c-tab folder
   * @param title title of tab
   * @return new composite widget
   */
  public static Composite addTab(CTabFolder cTabFolder, String title, boolean isVisible)
  {
    return addTab(cTabFolder,title,(Object)null,isVisible);
  }

  /** set tab widget
   * @param tabItem tab item
   * @param composite tab to set
   * @param title title of tab
   * @param data data element
   */
  public static void setTab(CTabItem cTabItem, Composite composite, String title, Object data)
  {
    cTabItem.setData(data);
    cTabItem.setText(title);
    cTabItem.setControl(composite);
  }

  /** set tab widget
   * @param tabItem tab item
   * @param composite tab to set
   * @param title title of tab
   */
  public static void setTab(CTabItem cTabItem, Composite composite, String title)
  {
    setTab(cTabItem,composite,title,null);
  }

  /** remove tab widget
   * @param tabFolder tab folder
   * @param composite tab to remove
   */
  public static void removeTab(CTabFolder cTabFolder, Composite composite)
  {
    CTabItem cTabItem = getTabItem(cTabFolder,composite);
    if (cTabItem != null)
    {
      composite.dispose();
      cTabItem.dispose();
    }
  }

  /** get tab composite array
   * @param tabFolder tab folder
   * @param title title of tab
   * @return tab composites array
   */
  public static Composite[] getTabList(CTabFolder cTabFolder)
  {
    CTabItem[] cTabItems = cTabFolder.getItems();
    Composite[] tabList = new Composite[cTabItems.length];
    for (int i = 0; i < cTabItems.length; i++)
    {
      tabList[i] = (Composite)cTabItems[i].getControl();
    }
    return tabList;
  }

  /** get tab item
   * @param tabFolder tab folder
   * @param composite tab to find
   * @param tab item or null if not found
   */
  public static CTabItem getTabItem(CTabFolder cTabFolder, Composite composite)
  {
    for (CTabItem cTabItem : cTabFolder.getItems())
    {
      if (cTabItem.getControl() == composite)
      {
        return cTabItem;
      }
    }
    return null;
  }

  /** move tab
   * @param tabFolder tab folder
   * @param tabItem tab item
   * @param newIndex new tab index (0..n)
   */
  public static void moveTab(CTabFolder cTabFolder, CTabItem cTabItem, int newIndex)
  {
    // save data
    int     style       = cTabItem.getStyle();
    String  title       = cTabItem.getText();
    Object  data        = cTabItem.getData();
    Control control     = cTabItem.getControl();
    String  toolTipText = cTabItem.getToolTipText();
    boolean selected    = (cTabFolder.getSelection() == cTabItem);

    // remove old tab
    cTabItem.dispose();

    // create tab a new position
    cTabItem = new CTabItem(cTabFolder,style,newIndex);

    // restore data
    cTabItem.setText(title);
    cTabItem.setData(data);
    cTabItem.setControl(control);
    cTabItem.setToolTipText(toolTipText);
    if (selected) cTabFolder.setSelection(cTabItem);
  }

  /** move tab
   * @param tabFolder tab folder
   * @param composite tab item
   * @param newIndex new tab index (0..n)
   */
  public static void moveTab(CTabFolder cTabFolder, Composite composite, int newIndex)
  {
    moveTab(cTabFolder,getTabItem(cTabFolder,composite),newIndex);
  }

  /** set tab widget title
   * @param tabFolder tab folder
   * @param composite tab item
   * @param title title to set
   */
  public static void setTabTitle(CTabFolder cTabFolder, Composite composite, String title)
  {
    CTabItem cTabItem = getTabItem(cTabFolder,composite);
    if (cTabItem != null)
    {
      cTabItem.setText(title);
    }
  }

  /** show tab
   * @param tabFolder tab folder
   * @param composite tab to show
   */
  public static void showTab(CTabFolder cTabFolder, Composite composite)
  {
    CTabItem cTabItem = getTabItem(cTabFolder,composite);
    if (cTabItem != null)
    {
      cTabFolder.setSelection(cTabItem);
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
   * @param data data structure
   * @param text menu text
   * @param isVisible true for visible, false otherwise
   * @return new menu
   */
  public static Menu insertMenu(Menu menu, int index, final Object data, String text, boolean isVisible)
  {
    Menu subMenu;

    if (isVisible)
    {
      MenuItem menuItem;
      if (index >= 0)
      {
        menuItem = new MenuItem(menu,SWT.CASCADE,index);
      }
      else
      {
        menuItem = new MenuItem(menu,SWT.CASCADE);
      }
      menuItem.setText(text);
      menuItem.setData(data);
      subMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
      subMenu.setData(data);
      menuItem.setMenu(subMenu);
    }
    else
    {
      subMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
      subMenu.setVisible(false);
    }

    return subMenu;
  }

  /** create new menu
   * @param menu menu bar
   * @param data data structure
   * @param text menu text
   * @return new menu
   */
  public static Menu insertMenu(Menu menu, int index, Object data, String text)
  {
    return insertMenu(menu,index,data,text,true);
  }

  /** create new menu
   * @param menu menu bar
   * @param data data structure
   * @param text menu text
   * @param isVisible true for visible, false otherwise
   * @return new menu
   */
  public static Menu addMenu(Menu menu, Object data, String text, boolean isVisible)
  {
    return insertMenu(menu,-1,data,text,isVisible);
  }

  /** create new menu
   * @param menu menu bar
   * @param data data structure
   * @param text menu text
   * @return new menu
   */
  public static Menu addMenu(Menu menu, Object data, String text)
  {
    return addMenu(menu,data,text,true);
  }

  /** create new menu
   * @param menu menu bar
   * @param text menu text
   * @param isVisible true for visible, false otherwise
   * @return new menu
   */
  public static Menu addMenu(Menu menu, String text, boolean isVisible)
  {
    return addMenu(menu,(Object)null,text,isVisible);
  }

  /** create new menu
   * @param menu menu bar
   * @param text menu text
   * @return new menu
   */
  public static Menu addMenu(Menu menu, String text)
  {
    return addMenu(menu,text,true);
  }

  /** get menu item
   * @param menu menu
   * @param data data structure
   * @param comparator data comparator
   * @return menu item
   */
  public static Menu getMenu(final Menu menu, final Object data, final Comparator comparator)
  {
    final Menu result[] = new Menu[]{null};

    menu.getDisplay().syncExec(new Runnable()
    {
      public void run()
      {
        for (MenuItem menuItem : menu.getItems())
        {
          Menu subMenu = menuItem.getMenu();
          if (subMenu != null)
          {
            if (comparator.compare(subMenu.getData(),data) == 0)
            {
              result[0] = subMenu;
              return;
            }
            else
            {
              subMenu = getMenu(subMenu,data,comparator);
              if (subMenu != null)
              {
                result[0] = subMenu;
                return;
              }
            }
          }
        }
      }
    });

    return result[0];
  }

  /** get menu item
   * @param menu menu
   * @param data data structure
   * @return menu item
   */
  public static Menu getMenu(final Menu menu, final Object data)
  {
    final Menu result[] = new Menu[]{null};

    menu.getDisplay().syncExec(new Runnable()
    {
      public void run()
      {
        for (MenuItem menuItem : menu.getItems())
        {
          Menu subMenu = menuItem.getMenu();
          if (subMenu != null)
          {
            if (subMenu.getData().equals(data))
            {
              result[0] = subMenu;
              return;
            }
            else
            {
              subMenu = getMenu(subMenu,data);
              if (subMenu != null)
              {
                result[0] = subMenu;
                return;
              }
            }
          }
        }
      }
    });

    return result[0];
  }

  /** remove menu item
   * @param menu menu
   * @param data data structure
   */
  public static void removeMenu(Menu menu, final Object data)
  {
    if ((menu.getData() != null) && (menu.getData().equals(data)))
    {
      menu.dispose();
    }
    else
    {
      Menu subMenu = getMenu(menu,data);
      if (subMenu != null)
      {
        menu.dispose();
      }
    }
  }

  /** insert new menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuItem(Menu menu, int index, final Object data, String text, int accelerator, boolean isVisible)
  {
    MenuItem menuItem;

    if (isVisible)
    {
      if (accelerator != SWT.NONE)
      {
        char key = (char)(accelerator & SWT.KEY_MASK);
        int acceleratorIndex = text.indexOf(key);
        if (acceleratorIndex >= 0)
        {
          text = text.substring(0,acceleratorIndex)+'&'+text.substring(acceleratorIndex);
        }
        text = text+"\t"+menuAcceleratorToText(accelerator);
      }
      if (index >= 0)
      {
        menuItem = new MenuItem(menu,SWT.DROP_DOWN,index);
      }
      else
      {
        menuItem = new MenuItem(menu,SWT.DROP_DOWN);
      }
      menuItem.setData(data);
      menuItem.setText(text);
      if (accelerator != SWT.NONE) menuItem.setAccelerator(accelerator);
    }
    else
    {
      Menu invisibleMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
      invisibleMenu.setVisible(false);
      menuItem = new MenuItem(invisibleMenu,SWT.DROP_DOWN);
    }

    return menuItem;
  }

  /** insert new menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem insertMenuItem(Menu menu, int index, Object data, String text, int accelerator)
  {
    return insertMenuItem(menu,index,data,text,accelerator,true);
  }

  /** insert new menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure
   * @param text menu item text
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuItem(Menu menu, int index, Object data, String text, boolean isVisible)
  {
    return insertMenuItem(menu,index,data,text,SWT.NONE,isVisible);
  }

  /** insert new menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem insertMenuItem(Menu menu, int index, Object data, String text)
  {
    return insertMenuItem(menu,index,data,text,true);
  }

  /** add new menu item
   * @param menu menu
   * @param data data structure
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, Object data, String text, int accelerator, boolean isVisible)
  {
    return insertMenuItem(menu,-1,data,text,accelerator,isVisible);
  }

  /** add new menu item
   * @param menu menu
   * @param data data structure
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, Object data, String text, int accelerator)
  {
    return addMenuItem(menu,data,text,accelerator,true);
  }

  /** add new menu item
   * @param menu menu
   * @param data data structure
   * @param text menu item text
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, Object data, String text, boolean isVisible)
  {
    return addMenuItem(menu,data,text,SWT.NONE,isVisible);
  }

  /** add new menu item
   * @param menu menu
   * @param data data structure
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, Object data, String text)
  {
    return addMenuItem(menu,data,text,true);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text, int accelerator, boolean isVisible)
  {
    return addMenuItem(menu,(Object)null,text,accelerator,isVisible);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text, int accelerator)
  {
    return addMenuItem(menu,text,accelerator,true);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text, boolean isVisible)
  {
    return addMenuItem(menu,text,SWT.NONE,isVisible);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuItem(Menu menu, String text)
  {
    return addMenuItem(menu,text,true);
  }

  /** insert new checkbox menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuCheckbox(Menu menu, int index, final Object data, String text, final String field, final Object value, int accelerator, boolean isVisible)
  {
    MenuItem menuItem;

    if (isVisible)
    {
      if (accelerator != SWT.NONE)
      {
        char key = (char)(accelerator & SWT.KEY_MASK);
        int acceleratorIndex = text.indexOf(key);
        if (acceleratorIndex >= 0)
        {
          text = text.substring(0,acceleratorIndex)+'&'+text.substring(acceleratorIndex);
        }
        text = text+"\t"+menuAcceleratorToText(accelerator);
      }
      if (index >= 0)
      {
        menuItem = new MenuItem(menu,SWT.CHECK,index);
      }
      else
      {
        menuItem = new MenuItem(menu,SWT.CHECK);
      }
      menuItem.setData(data);
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
    }
    else
    {
      Menu invisibleMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
      invisibleMenu.setVisible(false);
      menuItem = new MenuItem(invisibleMenu,SWT.CHECK);
    }

    return menuItem;
  }

  /** insert new checkbox menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuCheckbox(Menu menu, int index, Object data, String text, String field, Object value, boolean isVisible)
  {
    return insertMenuCheckbox(menu,index,data,text,field,value,isVisible);
  }

  /** insert new checkbox menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuCheckbox(Menu menu, int index, Object data, String text, String field, Object value)
  {
    return insertMenuCheckbox(menu,index,data,text,field,value,true);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, Object data, String text, String field, Object value, int accelerator, boolean isVisible)
  {
    return insertMenuCheckbox(menu,-1,data,text,field,value,accelerator,isVisible);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, Object data, String text, String field, Object value, int accelerator)
  {
    return addMenuCheckbox(menu,data,text,field,value,accelerator,true);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, Object data, String text, String field, Object value, boolean isVisible)
  {
    return addMenuCheckbox(menu,data,text,field,value,SWT.NONE,isVisible);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param data data structure to store checkbox value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, Object data, String text, String field, Object value)
  {
    return addMenuCheckbox(menu,data,text,field,value,true);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, int accelerator, boolean isVisible)
  {
    return addMenuCheckbox(menu,(Object)null,text,(String)null,(Object)null,accelerator,isVisible);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, int accelerator)
  {
    return addMenuCheckbox(menu,text,accelerator,true);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param text menu item text
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text, boolean isVisible)
  {
    return addMenuCheckbox(menu,text,SWT.NONE,isVisible);
  }

  /** add new checkbox menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuCheckbox(Menu menu, String text)
  {
    return addMenuCheckbox(menu,text,true);
  }

  /** add new radio menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuRadio(Menu menu, int index, final Object data, String text, final String field, final Object value, int accelerator, boolean isVisible)
  {
    MenuItem menuItem;

    if (isVisible)
    {
      if (accelerator != SWT.NONE)
      {
        char key = (char)(accelerator & SWT.KEY_MASK);
        int acceleratorIndex = text.indexOf(key);
        if (acceleratorIndex >= 0)
        {
          text = text.substring(0,acceleratorIndex)+'&'+text.substring(acceleratorIndex);
        }
        text = text+"\t"+menuAcceleratorToText(accelerator);
      }
      if (index >= 0)
      {
        menuItem = new MenuItem(menu,SWT.RADIO,index);
      }
      else
      {
        menuItem = new MenuItem(menu,SWT.RADIO);
      }
      menuItem.setData(data);
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
    }
    else
    {
      Menu invisibleMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
      invisibleMenu.setVisible(false);
      menuItem = new MenuItem(invisibleMenu,SWT.RADIO);
    }

    return menuItem;
  }

  /** add new radio menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem insertMenuRadio(Menu menu, int index, Object data, String text, String field, Object value, boolean isVisible)
  {
    return insertMenuRadio(menu,index,data,text,field,value,SWT.NONE,isVisible);
  }

  /** add new radio menu item
   * @param menu menu
   * @param index index [0..n-1] or -1
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new menu item
   */
  public static MenuItem insertMenuRadio(Menu menu, int index, Object data, String text, String field, Object value)
  {
    return insertMenuRadio(menu,index,data,text,field,value,true);
  }

  /** add new radio menu item
   * @param menu menu
   * @param data data structure to store radio value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param accelerator accelerator key or SWT.NONE
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, Object data, String text, String field, Object value, int accelerator, boolean isVisible)
  {
    return insertMenuRadio(menu,-1,data,text,field,value,accelerator,isVisible);
  }

  /** add new radio menu item
   * @param menu menu
   * @param data data structure to store radio value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param accelerator accelerator key or SWT.NONE
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text, Object data, String field, Object value, int accelerator)
  {
    return addMenuRadio(menu,data,text,field,value,accelerator,true);
  }

  /** add new radio menu item
   * @param menu menu
   * @param data data structure to store radio value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, Object data, String text, String field, Object value, boolean isVisible)
  {
    return addMenuRadio(menu,data,text,field,value,SWT.NONE,isVisible);
  }

  /** add new radio menu item
   * @param menu menu
   * @param data data structure to store radio value or null
   * @param text menu item text
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, Object data, String text, String field, Object value)
  {
    return addMenuRadio(menu,data,text,field,value,true);
  }

  /** add new radio menu item
   * @param menu menu
   * @param text menu item text
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text, boolean isVisible)
  {
    return addMenuRadio(menu,(Object)null,text,(String)null,(Object)null,isVisible);
  }

  /** add new radio menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  public static MenuItem addMenuRadio(Menu menu, String text)
  {
    return addMenuRadio(menu,text,true);
  }

  /** add new menu separator
   * @param menu menu
   * @param isVisible true for visible, false otherwise
   * @return new menu item
   */
  public static MenuItem addMenuSeparator(Menu menu, boolean isVisible)
  {
    MenuItem menuItem;

    if (isVisible)
    {
      menuItem = new MenuItem(menu,SWT.SEPARATOR);
    }
    else
    {
      menuItem = null;
    }

    return menuItem;
  }

  /** add new menu separator
   * @param menu menu
   * @return new menu item
   */
  public static MenuItem addMenuSeparator(Menu menu)
  {
    return addMenuSeparator(menu,true);
  }

  /** get menu item
   * @param menu menu
   * @param data data structure
   * @param comparator data comparator
   * @return menu item
   */
  public static MenuItem getMenuItem(final Menu menu, final Object data, final Comparator comparator)
  {
    final MenuItem result[] = new MenuItem[]{null};

    menu.getDisplay().syncExec(new Runnable()
    {
      public void run()
      {
        Menu     subMenu;
        MenuItem subMenuItem;

        for (MenuItem menuItem : menu.getItems())
        {
          if (comparator.compare(menuItem.getData(),data) == 0)
          {
            result[0] = menuItem;
            return;
          }
          else
          {
            subMenu = menuItem.getMenu();
            if (subMenu != null)
            {
              subMenuItem = getMenuItem(subMenu,data,comparator);
              if (subMenuItem != null)
              {
                result[0] = subMenuItem;
                return;
              }
            }
          }
        }
      }
    });

    return result[0];
  }

  /** get menu item
   * @param menu menu
   * @param data data structure
   * @return menu item
   */
  public static MenuItem getMenuItem(final Menu menu, final Object data)
  {
    final MenuItem result[] = new MenuItem[]{null};

    menu.getDisplay().syncExec(new Runnable()
    {
      public void run()
      {
        Menu     subMenu;
        MenuItem subMenuItem;

        for (MenuItem menuItem : menu.getItems())
        {

          if ((menuItem.getData() != null) && (menuItem.getData().equals(data)))
          {
            result[0] = menuItem;
            return;
          }
          else
          {
            subMenu = menuItem.getMenu();
            if (subMenu != null)
            {
              subMenuItem = getMenuItem(subMenu,data);
              if (subMenuItem != null)
              {
                result[0] = subMenuItem;
                return;
              }
            }
          }
        }
      }
    });

    return result[0];
  }

  /** update menu item
   * @param menu menu
   * @param data data structure
   * @param text text
   */
  public static void updateMenuItem(Menu menu, final Object data, String text)
  {
    MenuItem menuItem = getMenuItem(menu,data);
    if (menuItem != null)
    {
      menuItem.setText(text);
    }
  }

  /** remove menu item
   * @param menu menu
   * @param data data structure
   */
  public static void removeMenuItem(Menu menu, final Object data)
  {
    MenuItem menuItem = getMenuItem(menu,data);
    if (menuItem != null)
    {
      menuItem.dispose();
    }
  }

/** remove menu item
   * @param menu menu
   * @param menuItem menu item
   */
  public static void removeMenuItem(Menu menu, MenuItem menuItem)
  {
    menuItem.dispose();
  }

  /** remove menu item
   * @param menu menu
   * @param index menu item index [0..n-1]
   */
  public static void removeMenuItems(Menu menu, int fromIndex, int toIndex)
  {
    MenuItem[] menuItems = menu.getItems();
    if (toIndex < 0) toIndex = menuItems.length-1;
    for (int index = fromIndex; index <= toIndex; index++)
    {
      menuItems[index].dispose();
    }
  }

  /** remove menu item
   * @param menu menu
   * @param index menu item index [0..n-1]
   */
  public static void removeMenuItem(Menu menu, int index)
  {
    removeMenuItems(menu,index,index);
  }

  /** remove all menu items
   * @param menu menu
   */
  public static void removeAllMenuItems(Menu menu)
  {
    MenuItem[] menuItems = menu.getItems();
    for (int i = 0; i < menuItems.length; i++)
    {
      menuItems[i].dispose();
    }
  }


  //-----------------------------------------------------------------------

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @param margin margin or 0
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style, int margin, boolean isVisible)
  {
    Composite childComposite;

    childComposite = new Composite(composite,style);
    childComposite.setLayoutData(new TableLayoutData(isVisible));
    childComposite.setLayout(new TableLayout(margin));
    childComposite.setVisible(isVisible);

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
   * @param margin margin or 0
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style, int margin)
  {
    return newComposite(composite,style,margin,true);
  }

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style, boolean isVisible)
  {
    return newComposite(composite,style,0,isVisible);
  }

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, int style)
  {
    return newComposite(composite,style,true);
  }

  /** new composite widget
   * @param composite composite widget
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite, boolean isVisible)
  {
    return newComposite(composite,SWT.NONE,isVisible);
  }

  /** new composite widget
   * @param composite composite widget
   * @return new composite widget
   */
  public static Composite newComposite(Composite composite)
  {
    return newComposite(composite,true);
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

  /** new separator widget
   * @param composite composite widget
   * @param text separator text
   * @param style style
   * @return new separator widget
   */
  public static Separator newSeparator(Composite composite, String text, int style)
  {
    return new Separator(composite,text,style);
  }

  /** new separator widget
   * @param composite composite widget
   * @param text separator text
   * @return new separator widget
   */
  public static Separator newSeparator(Composite composite, String text)
  {
    return newSeparator(composite,text,SWT.NONE);
  }

  /** new separator widget
   * @param composite composite widget
   * @return new separator widget
   */
  public static Canvas newSeparator(Composite composite)
  {
    return newSeparator(composite,"");
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
    if (title != null) group.setText(title);
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

  /** new group widget
   * @param composite composite widget
   * @param style style
   * @param margin margin or 0
   * @return new group widget
   */
  public static Group newGroup(Composite composite, int style, int margin)
  {
    return newGroup(composite,(String)null,style,margin);
  }

  /** new group widget
   * @param composite composite widget
   * @param style style
   * @return new group widget
   */
  public static Group newGroup(Composite composite, int style)
  {
    return newGroup(composite,style,0);
  }

  /** new group widget
   * @param composite composite widget
   * @return new group widget
   */
  public static Group newGroup(Composite composite)
  {
    return newGroup(composite,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** add modify listener
   * @param widgetModifyListener listener to add
   */
  public static void addModifyListener(WidgetModifyListener widgetModifyListener)
  {
    widgetModifyListenerHashSet.add(widgetModifyListener);
    widgetModifyListener.modified();
  }

  /** remove modify listener
   * @param widgetModifyListener listener to remove
   */
  public static void removeModifyListener(WidgetModifyListener widgetModifyListener)
  {
    widgetModifyListenerHashSet.remove(widgetModifyListener);
  }

  /** execute modify listeners
   * @param variable modified variable
   */
  public static void modified(WidgetVariable widgetVariable)
  {
    for (WidgetModifyListener widgetModifyListener : widgetModifyListenerHashSet)
    {
      if (widgetModifyListener.equals(widgetVariable))
      {
        widgetModifyListener.modified();
      }
    }
  }

  /** execute modify listeners
   * @param variable modified variable
   */
  public static void modified(Object object)
  {
    for (WidgetModifyListener widgetModifyListener : widgetModifyListenerHashSet)
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

  /** signal notifcation of control and all sub-controls
   * @param receiver receiver widget
   * @param type event type to generate
   * @param event event
   */
  private static void notify(Widget receiver, int type, Event event)
  {
    if (!receiver.isDisposed())
    {
      if (receiver instanceof Control)
      {
        Control control = (Control)receiver;
        if (control.isEnabled())
        {
          control.notifyListeners(type,event);
          if (event.doit)
          {
            if (control instanceof Composite)
            {
              Composite composite = (Composite)control;
              if (!composite.isDisposed())
              {
                for (Control child : composite.getChildren())
                {
                  notify(child,type,event);
                  if (!event.doit) break;
                }
              }
            }
          }
        }
      }
      else
      {
        receiver.notifyListeners(type,event);
      }
    }
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param index index of event
   * @param text text of event
   * @param item item of event
   * @param data data of event
   */
  public static void notify(final Widget receiver, final int type, final Widget widget, final int index, final String text, final Widget item, final Object data)
  {
    if (!receiver.isDisposed())
    {
      receiver.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          Event event = new Event();
          event.widget = widget;
          event.index  = index;
          event.text   = text;
          event.item   = item;
          event.data   = data;
          Widgets.notify(receiver,type,event);
        }
      });
    }
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param index index of event
   * @param item item of event
   * @param data data of event
   */
  public static void notify(Widget receiver, int type, Widget widget, int index, Widget item, Object data)
  {
    notify(receiver,type,widget,index,(String)null,item,data);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param text text of event
   * @param item item of event
   * @param data data of event
   */
  public static void notify(Widget receiver, int type, Widget widget, String text, Widget item, Object data)
  {
    notify(receiver,type,widget,-1,text,item,data);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param index index of event
   * @param item item of event
   */
  public static void notify(Widget receiver, int type, Widget widget, int index, Widget item)
  {
    notify(receiver,type,widget,index,item,null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param text text of event
   * @param item item of event
   */
  public static void notify(Widget receiver, int type, Widget widget, String text, Widget item)
  {
    notify(receiver,type,widget,text,item,null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param index index of event
   * @param widget widget of event
   */
  public static void notify(Widget receiver, int type, int index, Widget widget)
  {
    notify(receiver,type,widget,index,null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param text text of event
   * @param widget widget of event
   */
  public static void notify(Widget receiver, int type, String text, Widget widget)
  {
    notify(receiver,type,widget,text,null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   * @param item item of event
   */
  public static void notify(Widget receiver, int type, Widget widget, Widget item)
  {
    notify(receiver,type,widget,-1,item);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param widget widget of event
   */
  public static void notify(Widget receiver, int type, Widget widget)
  {
    notify(receiver,type,widget,-1,null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param index index of event
   */
  public static void notify(Widget receiver, int type, int index)
  {
    notify(receiver,type,receiver,index,(Widget)null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param text text of event
   */
  public static void notify(Widget receiver, int type, String text)
  {
    notify(receiver,type,receiver,text,(Widget)null);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   * @param index index of event
   */
  public static void notify(Widget receiver, int type, Object data)
  {
    notify(receiver,type,receiver,-1,(String)null,(Widget)null,data);
  }

  /** signal notifcation
   * @param receiver receiver widget
   * @param type event type to generate
   */
  public static void notify(Widget receiver, int type)
  {
    notify(receiver,type,receiver);
  }

  /** signal notifcation
   * @param receiver receiver widget
   */
  public static void notify(Widget receiver)
  {
    notify(receiver,SWT.Selection);
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
