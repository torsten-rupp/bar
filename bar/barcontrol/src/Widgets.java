/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Widgets.java,v $
* $Revision: 1.19 $
* $Author: torsten $
* Contents: simple widgets functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.File;
import java.io.InputStream;
import java.net.URL;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;

// graphics
import org.eclipse.swt.custom.SashForm;
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
import org.eclipse.swt.graphics.GC;
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

  /** create BAR variable
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
   */
  void set(boolean b)
  {
    assert type == WidgetVariableTypes.BOOLEAN;

    this.b = b;
    Widgets.modified(this);
  }

  /** set long value
   * @param l value
   */
  void set(long l)
  {
    assert type == WidgetVariableTypes.LONG;

    this.l = l;
    Widgets.modified(this);
  }

  /** set double value
   * @param d value
   */
  void set(double d)
  {
    assert type == WidgetVariableTypes.DOUBLE;

    this.d = d;
    Widgets.modified(this);
  }

  /** set string value
   * @param string value
   */
  void set(String string)
  {
    assert (type == WidgetVariableTypes.STRING) || (type == WidgetVariableTypes.ENUMERATION);

    switch (type)
    {
      case STRING:
        this.string = string;
        break;
      case ENUMERATION:
        boolean OKFlag = false;
        for (String s : enumeration)
        {
          if (s.equals(string))
          {
            OKFlag = true;
          }
        }
        if (!OKFlag) throw new Error("Unknown enumeration value '"+string+"'");
        this.string = string;
        break;
    }
    Widgets.modified(this);
  }

  /** compare string values
   * @param value
   * @return true iff equal
   */
  public boolean equals(String value)
  {
    String s = toString();
    return (s != null)?s.equals(value):(value == null);
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
    }
    return "";
  }
}

/** widget listener
 */
class WidgetListener
{
  private WidgetVariable variable;
  private Control        control;

  // cached text for widget
  private String cachedText = null;

  /** create widget listener
   */
  WidgetListener()
  {
    this.control  = null;
    this.variable = null;
  }

  /** create widget listener
   * @param control control widget
   * @param variable BAR variable
   */
  WidgetListener(Control control, WidgetVariable variable)
  {
    this.control  = control;
    this.variable = variable;
  }

  /** set control widget
   * @param control control widget
   */
  void setControl(Control control)
  {
    this.control = control;
  }

  /** set variable
   * @param variable BAR variable
   * @return 
   */
  void setVariable(WidgetVariable variable)
  {
    this.variable = variable;
  }

  /** compare variables
   * @param variable variable
   * @return true iff equal
   */
  public boolean equals(Object variable)
  {
    return (this.variable != null) && this.variable.equals(variable);
  }

  /** nofity modify variable
   * @param control control widget
   * @param variable BAR variable
   */
  void modified(Control control, WidgetVariable variable)
  {
    if      (control instanceof Label)
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
      if (!text.equals(cachedText))
      {
        ((Label)control).setText(text);
        control.getParent().layout(true,true);
        cachedText = text;
      }
    }
    else if (control instanceof Button)
    {
      if      ((((Button)control).getStyle() & SWT.PUSH) == SWT.PUSH)
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
        if (!text.equals(cachedText))
        {
          ((Button)control).setText(text);
          control.getParent().layout();
          cachedText = text;
        }
      }
      else if ((((Button)control).getStyle() & SWT.CHECK) == SWT.CHECK)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break; 
          case DOUBLE:  selection = (variable.getDouble() != 0); break; 
        }
        ((Button)control).setSelection(selection);
      }
      else if ((((Button)control).getStyle() & SWT.RADIO) == SWT.RADIO)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break; 
          case DOUBLE:  selection = (variable.getDouble() != 0); break; 
        }
        ((Button)control).setSelection(selection);
      }
    }
    else if (control instanceof Combo)
    {
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
      if (!text.equals(cachedText))
      {
        ((Combo)control).setText(text);
        control.getParent().layout();
        cachedText = text;
      }
    }
    else if (control instanceof Text)
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
      if (!text.equals(cachedText))
      {
        ((Text)control).setText(text);
        control.getParent().layout();
        cachedText = text;
      }
    }
    else if (control instanceof Spinner)
    {
      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break; 
        case DOUBLE: n = (int)variable.getDouble(); break; 
      }
      ((Spinner)control).setSelection(n);
    }
    else if (control instanceof Slider)
    {
      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break; 
        case DOUBLE: n = (int)variable.getDouble(); break; 
      }
      ((Slider)control).setSelection(n);
    }
    else if (control instanceof Scale)
    {
      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break; 
        case DOUBLE: n = (int)variable.getDouble(); break; 
      }
      ((Scale)control).setSelection(n);
    }
    else if (control instanceof ProgressBar)
    {
      double value = 0;
      switch (variable.getType())
      {
        case LONG:   value = (double)variable.getLong(); break; 
        case DOUBLE: value = variable.getDouble(); break; 
      }
      ((ProgressBar)control).setSelection(value);
    }
    else
    {
      throw new InternalError("Unknown widget '"+control+"' in wiget listener!");
    }
  }

  /** notify modify variable
   * @param variable BAR variable
   */
  public void modified(WidgetVariable variable)
  {
    modified(control,variable);
  }

  /** notify modify variable
   */
  public void modified()
  {
    modified(variable);
  }

  /** get string of varable
   * @param variable BAR variable
   */
  String getString(WidgetVariable variable)
  {
    return null;
  }
}

/** widget column table data
 */
class WidgetTableColumnData
{
  public int     width;
  public boolean resizable;

  /** create column data
   * @param width column width
   * @param resizable true iff column is resiable
   */
  WidgetTableColumnData(int width, boolean resizable)
  {
    this.width     = width;
    this.resizable = resizable;
  }
}

class Widgets
{
  //-----------------------------------------------------------------------

  /** list of widgets listeners
   */
  private static LinkedList<WidgetListener> listenersList = new LinkedList<WidgetListener>();

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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight, int maxWidth, int maxHeight)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, Point size)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point pad, Point size)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point size)
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
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad,pad);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,0);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   */
  static void layout(Control control, int row, int column, int style)
  {
//    layout(control,row,column,style,0,0);
    layout(control,row,column,style,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** get text height
   * @param control control
   * @return height of text
   */
  static int getTextHeight(Control control)
  {
    int height;

    GC gc = new GC(control);
    height = gc.getFontMetrics().getHeight();
    gc.dispose();

    return height;
  }

  /** get text size
   * @param control control
   * @return size of text
   */
  static Point getTextSize(Control control, String text)
  {
    Point size;

    GC gc = new GC(control);
    size = gc.textExtent(text);
    gc.dispose();

    return size;
  }

  /** get max. text size
   * @param control control
   * @return max. size of all texts
   */
  static Point getTextSize(Control control, String[] texts)
  {
    Point size;

    size = new Point(0,0);
    GC gc = new GC(control);
    for (String text : texts)
    {
      size.x = Math.max(size.x,gc.textExtent(text).x);
      size.y = Math.max(size.y,gc.textExtent(text).y);
    }
    gc.dispose();

    return size;
  }

  /** load image from jar or directory "images"
   * @param fileName image file name
   * @return image
   */
  static Image loadImage(Display display, String fileName)
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

  /** get accelerator text
   * @param accelerator SWT accelerator
   * @return accelerator text
   */
  static String acceleratorToText(int accelerator)
  {
    StringBuffer text = new StringBuffer();

    if (accelerator != 0)
    {
      if ((accelerator & SWT.MOD1) == SWT.CTRL) text.append("Ctrl+");
      if ((accelerator & SWT.MOD2) == SWT.ALT ) text.append("Alt+");

      if      ((accelerator & SWT.KEY_MASK) == SWT.F1 ) text.append("F1");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F2 ) text.append("F2");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F3 ) text.append("F3");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F4 ) text.append("F4");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F5 ) text.append("F5");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F6 ) text.append("F6");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F7 ) text.append("F7");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F8 ) text.append("F8");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F9 ) text.append("F9");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F10) text.append("F10");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F11) text.append("F11");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F12) text.append("F12");
      else                                              text.append((char)(accelerator & SWT.KEY_MASK));
    }

    return text.toString();
  }

  /** set enabled
   * @param control control to enable/disable
   * @param enableFlag true to enable, false to disable
   */
  static void setEnabled(Control control, boolean enableFlag)
  {
    control.setEnabled(enableFlag);
  }

  /** set visible
   * @param control control to make visible/invisible
   * @param visibleFlag true to make visible, false to make invisible
   */
  static void setVisible(Control control, boolean visibleFlag)
  {
    TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
    tableLayoutData.exclude = !visibleFlag;
    control.setVisible(visibleFlag);
    if (visibleFlag)
    {
      control.getParent().layout();
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

  public static void invoke(Button button)
  {
    Event event = new Event();
    event.widget = button;

    button.notifyListeners(SWT.Selection,event);
  }

  //-----------------------------------------------------------------------

  /** create empty space
   * @param composite composite widget
   * @return control space control
   */
  static Control newSpacer(Composite composite)
  {
    Label label = new Label(composite,SWT.NONE);

    return (Control)label;
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @param style label style
   * @return new label
   */
  static Label newLabel(Composite composite, String text, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setText(text);

    return label;
  }

  /** create new label
   * @param composite composite widget
   * @param text label text
   * @return new label
   */
  static Label newLabel(Composite composite, String text)
  {
    return newLabel(composite,text,SWT.LEFT);
  }

  /** create new label
   * @param composite composite widget
   * @return new label
   */
  static Label newLabel(Composite composite)
  {
    return newLabel(composite,"");
  }

  /** create new image
   * @param composite composite widget
   * @param image image
   * @param style label style
   * @return new image
   */
  static Control newImage(Composite composite, Image image, int style)
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
  static Control newImage(Composite composite, Image image)
  {
    return newImage(composite,image,SWT.LEFT);
  }

  /** create new view
   * @param composite composite widget
   * @param text view text
   * @param style view style
   * @return new view
   */
  static Label newView(Composite composite, String text, int style)
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
  static Label newView(Composite composite, String text)
  {
    return newView(composite,text,SWT.NONE);
  }

  /** create new view
   * @param composite composite widget
   * @return new view
   */
  static Label newView(Composite composite)
  {
    return newView(composite,"");
  }

  /** create new number view
   * @param composite composite widget
   * @return new view
   */
  static Label newNumberView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.RIGHT|SWT.BORDER);
    label.setText("0");

    return label;
  }

  /** create new string view
   * @param composite composite widget
   * @return new view
   */
  static Label newStringView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText("");

    return label;
  }

  /** create new button
   * @param composite composite widget
   * @param text text
   * @return new button
   */
  static Button newButton(Composite composite, String text)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setText(text);

    return button;
  }

  /** create new button with image
   * @param composite composite widget
   * @param image image
   * @return new button
   */
  static Button newButton(Composite composite, Image image)
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
  static Button newButton(Composite composite, Image image, String text)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setImage(image);
    button.setText(text);

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
  static Button newCheckbox(Composite composite, String text, final Object data, final String field, boolean value)
  {
    Button button;

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
   * @return new checkbox button
   */
  static Button newCheckbox(Composite composite, String text, final Object data, final String field)
  {
    return newCheckbox(composite,text,data,field,false);
  }

  /** create new checkbox
   * @param composite composite widget
   * @param text text
   * @return new checkbox button
   */
  static Button newCheckbox(Composite composite, String text)
  {
    return newCheckbox(composite,text,null,null);
  }

  /** create new checkbox
   * @param composite composite widget
   * @return new checkbox button
   */
  static Button newCheckbox(Composite composite)
  {
    return newCheckbox(composite,null);
  }

  /** create new radio button
   * @param composite composite widget
   * @param text text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @return new button
   */
  static Button newRadio(Composite composite, String text, final Object data, final String field, final Object value)
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
  static Button newRadio(Composite composite, String text)
  {
    return newRadio(composite,text,null,null,null);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @param value value for text input field
   * @param style text style
   * @return new text widget
   */
  static Text newText(Composite composite, final Object data, final String field, String value, int style)
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
  static Text newText(Composite composite, final Object data, final String field, String value)
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
  static Text newText(Composite composite, final Object data, final String field, int style)
  {
    return newText(composite,data,field,"",style);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param data data structure to store text value or null
   * @param field field name in data structure to set on selection
   * @return new text widget
   */
  static Text newText(Composite composite, final Object data, final String field)
  {
    return newText(composite,data,field,"");
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @param style text style
   * @return new text widget
   */
  static Text newText(Composite composite, int style)
  {
    return newText(composite,null,null,style);
  }

  /** create new text input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  static Text newText(Composite composite)
  {
    return newText(composite,null,null);
  }

  /** create new password input widget (single line)
   * @param composite composite widget
   * @return new text widget
   */
  static Text newPassword(Composite composite)
  {
    Text text;

    text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);

    return text;
  }

  /** create new list widget
   * @param composite composite widget
   * @param style style
   * @return new list widget
   */
  static List newList(Composite composite, int style)
  {
    List list;

    list = new List(composite,style);

    return list;
  }

  /** create new list widget
   * @param composite composite widget
   * @return new list widget
   */
  static List newList(Composite composite)
  {
    return newList(composite,SWT.BORDER|SWT.MULTI|SWT.V_SCROLL);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store combo value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new combo widget
   */
  static Combo newCombo(Composite composite, final Object data, final String field, String value)
  {
    Combo combo;

    combo = new Combo(composite,SWT.BORDER);
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
   * @return new combo widget
   */
  static Combo newCombo(Composite composite, final Object data, final String field)
  {
    return newCombo(composite,data,field,null);
  }

  /** new combo widget
   * @param composite composite widget
   * @return new combo widget
   */
  static Combo newCombo(Composite composite)
  {
    return newCombo(composite,null,null);
  }

  /** new combo widget
   * @param composite composite widget
   * @param data data structure to store select value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox
   * @return new select widget
   */
  static Combo newSelect(Composite composite, final Object data, final String field, String value)
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
  static Combo newSelect(Composite composite, final Object data, final String field)
  {
    return newSelect(composite,data,field,null);
  }

  /** new combo widget
   * @param composite composite widget
   * @return new select widget
   */
  static Combo newSelect(Composite composite)
  {
    return newSelect(composite,null,null);
  }

  /** create new option menu
   * @param composite composite widget
   * @return new combo widget
   */
  static Combo newOptionMenu(Composite composite)
  {
    Combo combo;

    combo = new Combo(composite,SWT.RIGHT|SWT.READ_ONLY);

    return combo;
  }

  /** create new spinner widget
   * @param composite composite widget
   * @return new spinner widget
   */
  static Spinner newSpinner(Composite composite)
  {
    Spinner spinner;

    spinner = new Spinner(composite,SWT.READ_ONLY);

    return spinner;
  }

  /** create new table widget
   * @param composite composite widget
   * @param style style
   * @param object object data
   * @return new table widget
   */
  static Table newTable(Composite composite, int style)
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
  static Table newTable(Composite composite)
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
  static TableColumn addTableColumn(Table table, int columnNb, String title, int style, int width, boolean resizable)
  {
    TableColumn tableColumn = new TableColumn(table,style);
    tableColumn.setText(title);
    tableColumn.setData(new WidgetTableColumnData(width,resizable));
    tableColumn.setWidth(width);
    tableColumn.setResizable(resizable);
    if (width <= 0) tableColumn.pack();

    return tableColumn;
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  static TableColumn addTableColumn(Table table, int columnNb, int style, int width)
  {
    return addTableColumn(table,columnNb,"",style,width,false);
  }

  /** hide table column
   * @param tableColumn table column to hide
   */
  static void hideTableColumn(TableColumn tableColumn)
  {
    tableColumn.setWidth(0);
    tableColumn.setResizable(false);
  }

  /** show table column
   * @param tableColumn table column to show
   * @param width table column width
   */
  static void showTableColumn(TableColumn tableColumn)
  {
    WidgetTableColumnData widgetTableColumnData = (WidgetTableColumnData)tableColumn.getData();

    if (widgetTableColumnData != null)
    {
      tableColumn.setWidth(widgetTableColumnData.width);
      tableColumn.setResizable(widgetTableColumnData.resizable);
    }
  }

  /** default table sort selection listener
   */
  final static SelectionListener DEFAULT_TABLE_SELECTION_LISTENER = new SelectionListener()
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

  /** sort table column
   * @param table table
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  static void sortTableColumn(Table table, TableColumn tableColumn, Comparator comparator)
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

  /** sort table column
   * @param table table
   * @param comparator table data comparator
   */
  static void sortTableColumn(Table table, Comparator comparator)
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
            String[] texts = new String[table.getColumnCount()];
            for (int z = 0; z < table.getColumnCount(); z++)
            {
              texts[z] = tableItems[i].getText(z);
            }
            Color foregroundColor = tableItems[i].getForeground();
            Color backgroundColor = tableItems[i].getBackground();
            boolean checked = tableItems[i].getChecked();

            // discard item
            tableItems[i].dispose();

            // create new item
            TableItem tableItem = new TableItem(table,SWT.NONE,j);
            tableItem.setData(data);
            tableItem.setText(texts);
            tableItem.setChecked(checked);
            tableItem.setForeground(foregroundColor);
            tableItem.setBackground(backgroundColor);

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
  static void sortTable(Table table, TableColumn tableColumn, int sortDirection)
  {
    Event event = new Event();

    table.setSortDirection(sortDirection);
    event.widget = tableColumn;
    tableColumn.notifyListeners(SWT.Selection,event);
  }

  /** sort table column
   * @param table table
   * @param n column index (0..n-1)
   * @param sortDirection sorting direction
   */
  static void sortTable(Table table, int n, int sortDirection)
  {
    sortTable(table,table.getColumn(n),sortDirection);
  }

  /** sort table column
   * @param table table
   * @param n column index (0..n-1)
   */
  static void sortTable(Table table, int n)
  {
    sortTable(table,n,SWT.UP);
  }

  /** get insert position in sorted table
   * @param table table
   * @param comparator table data comparator
   * @param data data
   * @return index in table
   */
  static int getTableItemIndex(Table table, Comparator comparator, Object data)
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
  static int insertTableEntry(final Table table, final int index, final Object data, final Object... values)
  {
    /** table insert runnable
     */
    class TableRunnable implements Runnable
    {
      int insertIndex = -1;

      public void run()
      {
        if (!table.isDisposed())
        {
          TableItem tableItem;
          if (index >= 0)
          {
            insertIndex = index;
            tableItem = new TableItem(table,SWT.NONE,index);
          }
          else
          {
            insertIndex = table.getItemCount();
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

    return tableRunnable.insertIndex;
  }

  /** add table entry
   * @param table table
   * @param comparator table entry comperator
   * @param table entry data
   * @param values values list
   * @return insert index
   */
  static int insertTableEntry(final Table table, final Comparator comparator, final Object data, final Object... values)
  {
    /** table insert runnable
     */
    class TableRunnable implements Runnable
    {
      int insertIndex = -1;

      public void run()
      {
        if (!table.isDisposed())
        {
          TableItem tableItem = new TableItem(table,
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

    return tableRunnable.insertIndex;
  }

  /** add table entry
   * @param table table
   * @param table entry data
   * @param values values list
   * @return insert index
   */
  static int addTableEntry(Table table, Object data, Object... values)
  {
    return insertTableEntry(table,-1,data,values);
  }

  /** update table entry
   * @param table table
   * @param data entry data
   * @param values values list
   * @param true if updated, false if not found
   */
  static boolean updateTableEntry(final Table table, final Object data, final Object... values)
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
  static void setTableEntryColor(final Table table, final Object data, final Color foregroundColor, final Color backgroundColor)
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
  static void setTableEntryColor(Table table, Object data, Color backgroundColor)
  {
    setTableEntryColor(table,data,null,backgroundColor);
  }

  /** set table entry checked
   * @param table table
   * @param table entry data
   * @param checked checked flag
   */
  static void setTableEntryChecked(final Table table, final Object data, final boolean checked)
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
  static void removeTableEntry(final Table table, final Object data)
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
  static void removeTableEntry(final Table table, final TableItem tableItem)
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

  /** remove all table entries
   * @param table table
   */
  static void removeAllTableEntries(final Table table)
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

  /** new progress bar widget
   * @param composite composite widget
   * @param min, max min/max value
   * @return new progress bar widget
   */
  static ProgressBar newProgressBar(Composite composite, double min, double max)
  {
    ProgressBar progressBar;

    progressBar = new ProgressBar(composite,SWT.HORIZONTAL);
    progressBar.setMinimum(min);
    progressBar.setMaximum(max);
    progressBar.setSelection(min);

    return progressBar;
  }

  /** new progress bar widget
   * @param composite composite widget
   * @return new progress bar widget
   */
  static ProgressBar newProgressBar(Composite composite)
  {
    return newProgressBar(composite,0.0,100.0);
  }

  /** set value of progress bar widget
   * @param progressBar progress bar
   * @param value value
   */
  static void setProgressBar(final ProgressBar progressBar, final double value)
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

  /** new tree widget
   * @param composite composite widget
   * @param style style
   * @return new tree widget
   */
  static Tree newTree(Composite composite, int style)
  {
    Tree tree = new Tree(composite,style|SWT.BORDER|SWT.H_SCROLL|SWT.V_SCROLL);
    tree.setHeaderVisible(true);

    return tree;
  }

  /** new tree widget
   * @param composite composite widget
   * @return new tree widget
   */
  static Tree newTree(Composite composite)
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
  static TreeColumn addTreeColumn(Tree tree, String title, int style, int width, boolean resizable)
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
  static TreeItem addTreeItem(Tree tree, int index, Object data, boolean folderFlag)
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
  static TreeItem addTreeItem(Tree tree, Object data, boolean folderFlag)
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
  static TreeItem addTreeItem(TreeItem parentTreeItem, int index, Object data, boolean folderFlag)
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
  static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, boolean folderFlag)
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
      boolean checked = treeItem.getChecked();
      Image image = treeItem.getImage();

      // recreate item
      if (parentTreeItem != null) newTreeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
      else                        newTreeItem = new TreeItem(tree,SWT.NONE,index);
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

  /** get expanded (open) directories in tree
   * @param expandedDirectories hash-set for expanded directories
   * @param treeItem tree item to start
   */
  private static void getExpandedDiretories(HashSet expandedDirectories, TreeItem treeItem)
  {
    if (!treeItem.isDisposed())
    {
      if (treeItem.getExpanded()) expandedDirectories.add(treeItem.getData());
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        getExpandedDiretories(expandedDirectories,subTreeItem);
      }
    }
  }  

  /** re-expand directories
   * @param expandedDirectories directories to re-expand
   * @return treeItem tree item to start
   */
  private static void reExpandDiretories(HashSet expandedDirectories, TreeItem treeItem)
  {
    if (!treeItem.isDisposed())
    {
      treeItem.setExpanded(expandedDirectories.contains(treeItem.getData()));
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        reExpandDiretories(expandedDirectories,subTreeItem);
      }
    }
  }  

  /** sort tree column
   * @param tree tree
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  static void sortTreeColumn(Tree tree, TreeColumn treeColumn, Comparator comparator)
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
      HashSet expandedDirectories = new HashSet();
      for (TreeItem treeItem : tree.getItems())
      {
        getExpandedDiretories(expandedDirectories,treeItem);
      }
//System.err.println("BARControl.java"+", "+1627+": "+expandedDirectories.toString());

      // sort column
//printTree(tree);
      sortTreeColumn(tree,tree.getItems(),sortDirection,comparator);

      // restore expanded sub-trees
      for (TreeItem treeItem : tree.getItems())
      {
        reExpandDiretories(expandedDirectories,treeItem);
      }

      // set column sort indicators
      tree.setSortColumn(treeColumn);
      tree.setSortDirection(sortDirection);
//System.err.println("2 ---------------");
//printTree(tree);
    }
  }

  /** create new sash widget (pane)
   * @param composite composite widget
   * @return new sash widget
   */
  static Sash newSash(Composite composite, int style)
  {    
    Sash sash = new Sash(composite,style);

    return sash;
  }

  /** create new sash form widget
   * @param composite composite widget
   * @return new sash form widget
   */
  static SashForm newSashForm(Composite composite, int style)
  {    
    SashForm sashForm = new SashForm(composite,style);

    return sashForm;
  }

  /** create new pane widget
   * @param composite composite widget
   * @param style style
   * @param prevPane previous pane
   * @return new pane widget
   */
  static Pane newPane(Composite composite, int style, Pane prevPane)
  {    
    Pane pane = new Pane(composite,style,prevPane);

    return pane;
  }

  /** create new pane widget
   * @param composite composite widget
   * @param style style
   * @return new pane widget
   */
  static Pane newPane(Composite composite, int style)
  {    
    Pane pane = new Pane(composite,style,null);

    return pane;
  }

  /** create new tab folder
   * @param compositet composite
   * @return new tab folder widget
   */
  static TabFolder newTabFolder(Composite composite)
  {    
    TabFolder tabFolder = new TabFolder(composite,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));

    return tabFolder;
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  static Composite addTab(TabFolder tabFolder, String title)
  {
    TabItem tabItem = new TabItem(tabFolder,SWT.NONE);
    tabItem.setText(title);
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

  /** show tab
   * @param tabFolder tab folder
   * @param composite tab to show
   */
  static void showTab(TabFolder tabFolder, Composite composite)
  {
    for (TabItem tabItem : tabFolder.getItems())
    {
      if (tabItem.getControl() == composite)
      {
        tabFolder.setSelection(tabItem);
        break;
      }
    }
  }

  /** create new canvas widget
   * @param composite composite
   * @param style style
   * @return new canvas widget
   */
  static Canvas newCanvas(Composite composite, int style)
  {    
    Canvas canvas = new Canvas(composite,style);
    /* canvas is a composite; set default layout */
    canvas.setLayout(new TableLayout(0.0,0.0,0));

    return canvas;
  }

  /** new slider widget
   * @param composite composite widget
   * @param style style
   * @return new group widget
   */
  static Slider newSlider(Composite composite, int style)
  {
    Slider slider = new Slider(composite,style);

    return slider;
  }

  /** new scale widget
   * @param composite composite widget
   * @param style style
   * @return new group widget
   */
  static Scale newScale(Composite composite, int style)
  {
    Scale scale = new Scale(composite,style);

    return scale;
  }

  /** create new menu bar
   * @param shell shell
   * @return new menu bar
   */
  static Menu newMenuBar(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.BAR);
    shell.setMenuBar(menu);

    return menu;
  }

  /** create new popup bar
   * @param shell shell
   * @return new popup menu
   */
  static Menu newPopupMenu(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.POP_UP);

    return menu;
  }

  /** create new menu
   * @param menu menu bar
   * @param text menu text
   * @return new menu
   */
  static Menu addMenu(Menu menu, String text)
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
   * @param accelerator accelerator key or 0
   * @return new menu item
   */
  static MenuItem addMenuItem(Menu menu, String text, int accelerator)
  {
    if (accelerator != 0)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+acceleratorToText(accelerator);
    }
    MenuItem menuItem = new MenuItem(menu,SWT.DROP_DOWN);
    menuItem.setText(text);
    if (accelerator != 0) menuItem.setAccelerator(accelerator);

    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  static MenuItem addMenuItem(Menu menu, String text)
  {
    return addMenuItem(menu,text,0);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store checkbox value or null
   * @param field field name in data structure to set on selection
   * @param value value for checkbox button
   * @param accelerator accelerator key or 0
   * @return new menu item
   */
  static MenuItem addMenuCheckbox(Menu menu, String text, final Object data, final String field, final Object value, int accelerator)
  {
    if (accelerator != 0)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+acceleratorToText(accelerator);
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
   * @param accelerator accelerator key or 0
   * @return new menu item
   */
  static MenuItem addMenuCheckbox(Menu menu, String text, final Object data, final String field, final Object value)
  {
    return addMenuCheckbox(menu,text,data,field,value,0);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param selected true iff checkbox menu entry is selected
   * @return new menu item
   */
  static MenuItem addMenuCheckbox(Menu menu, String text, boolean selected)
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
  static MenuItem addMenuCheckbox(Menu menu, String text)
  {
    return addMenuCheckbox(menu,text,null,null,null);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param data data structure to store radio value or null
   * @param field field name in data structure to set on selection
   * @param value value for radio button
   * @param accelerator accelerator key or 0
   * @return new menu item
   */
  static MenuItem addMenuRadio(Menu menu, String text, final Object data, final String field, final Object value, int accelerator)
  {
    if (accelerator != 0)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+acceleratorToText(accelerator);
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
    if (accelerator != 0) menuItem.setAccelerator(accelerator);

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
  static MenuItem addMenuRadio(Menu menu, String text, final Object data, final String field, final Object value)
  {
    return addMenuRadio(menu,text,data,field,value,0);
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param selected true iff radio menu entry is selected
   * @return new menu item
   */
  static MenuItem addMenuRadio(Menu menu, String text, boolean selected)
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
  static MenuItem addMenuRadio(Menu menu, String text)
  {
    return addMenuRadio(menu,text,null,null,null);
  }

  /** add new menu separator
   * @param menu menu
   * @return new menu item
   */
  static MenuItem addMenuSeparator(Menu menu)
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
  static Composite newComposite(Composite composite, int style, int margin)
  {
    Composite childComposite;

    childComposite = new Composite(composite,style);
    TableLayout tableLayout = new TableLayout(margin);
    childComposite.setLayout(tableLayout);

    return childComposite;
  }

  /** new composite widget
   * @param composite composite widget
   * @param style style
   * @return new composite widget
   */
  static Composite newComposite(Composite composite, int style)
  {
    return newComposite(composite,style,0);
  }

  /** new composite widget
   * @param composite composite widget
   * @return new composite widget
   */
  static Composite newComposite(Composite composite)
  {
    return newComposite(composite,SWT.NONE,0);
  }

  /** new group widget
   * @param composite composite widget
   * @param title group title
   * @param style style
   * @param margin margin or 0
   * @return new group widget
   */
  static Group newGroup(Composite composite, String title, int style, int margin)
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
  static Group newGroup(Composite composite, String title, int style)
  {
    return newGroup(composite,title,style,0);
  }

  /** new group widget
   * @param composite composite widget
   * @param title group title
   * @return new group widget
   */
  static Group newGroup(Composite composite, String title)
  {
    return newGroup(composite,title,SWT.NONE);
  }

  //-----------------------------------------------------------------------

  /** add modify listener
   * @param widgetListener listener to add
   */
  static void addModifyListener(WidgetListener widgetListener)
  {
    listenersList.add(widgetListener);
  }

  /** execute modify listeners
   * @param variable modified variable
   */
  static void modified(Object variable)
  {
    for (WidgetListener widgetListener : listenersList)
    {
      if (widgetListener.equals(variable))
      {
        widgetListener.modified();
      }
    }
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   * @param item item of event
   */
  static void notify(Control control, int type, Widget widget, Widget item)
  {
    if (control.isEnabled())
    {
      Event event = new Event();
      event.widget = widget;
      event.item   = item;
      control.notifyListeners(type,event);
    }
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   */
  static void notify(Control control, int type, Widget widget)
  {
    notify(control,type,widget,null);
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   */
  static void notify(Control control, int type)
  {
    notify(control,type,control,null);
  }

  /** signal modified
   * @param control control
   */
  static void notify(Control control)
  {
    notify(control,SWT.Selection,control,null);
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
  public static void setClipboard(Clipboard clipboard, String texts[])
  {
    StringBuffer buffer = new StringBuffer();
    String lineSeparator = System.getProperty("line.separator");
    for (String text : texts)
    {
      buffer.append(text);
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

  /** set clipboard with text from label
   * @param clipboard clipboard
   * @param label label
   */
  public static void setClipboard(Clipboard clipboard, Text text)
  {
    setClipboard(clipboard,text.getSelectionText());
  }

  /** set clipboard with text from table item
   * @param clipboard clipboard
   * @param tableItem table item
   */
  public static void setClipboard(Clipboard clipboard, TableItem tableItem)
  {
    Table table       = tableItem.getParent();
    int   columnCount = table.getColumnCount();

    StringBuffer buffer = new StringBuffer();
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
   */
  public static void setClipboard(Clipboard clipboard, TableItem[] tableItems)
  {
    if (tableItems.length > 0)
    {
      Table table       = tableItems[0].getParent();
      int   columnCount = Math.max(table.getColumnCount(),1);
      //if (columnCount==0) columnCount=1;

      StringBuffer buffer = new StringBuffer();
      for (int z = 0; z < tableItems.length ; z++)
      {
        if (z > 0) buffer.append('\n');
        for (int i = 0; i < columnCount ; i++)
        {
          if (i > 0) buffer.append('\t');
          buffer.append(tableItems[z].getText(i));
        }
      }

      setClipboard(clipboard,buffer.toString());
    }
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
}

/* end of file */
