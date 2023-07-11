/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: BAR special widgets
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.EnumSet;

import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/**
 * BAR special widgets
 */
public class BARWidgets
{
  /** listener
   */
  static class Listener
  {
    /** get int value
     * @param widgetVariable widget variable
     * @return value
     */
    public int getInt(WidgetVariable widgetVariable)
    {
      return 0;
    }

    /** set int value
     * @param widgetVariable widget variable
     * @param value value
     */
    public void setInt(WidgetVariable widgetVariable, int value)
    {
    }

    /** get long value
     * @param widgetVariable widget variable
     * @return value
     */
    public long getLong(WidgetVariable widgetVariable)
    {
      return 0L;
    }

    /** set long value
     * @param widgetVariable widget variable
     * @param value value
     */
    public void setLong(WidgetVariable widgetVariable, long value)
    {
    }

    /** get string value
     * @param widgetVariable widget variable
     * @return value
     */
    public String getString(WidgetVariable widgetVariable)
    {
      return "";
    }

    /** set string value
     * @param widgetVariable widget variable
     * @param value value
     */
    public void setString(WidgetVariable widgetVariable, String string)
    {
    }

    /** get checked state
     * @param widgetVariable widget variable
     * @return true iff checked
     */
    public boolean getChecked(WidgetVariable widgetVariable)
    {
      return false;
    }

    /** set checked state
     * @param widgetVariable
     * @param checked true iff checked
     */
    public void setChecked(WidgetVariable widgetVariable, boolean checked)
    {
    }
  }

  // colors
  private final static Color COLOR_MODIFIED  = new Color(null,0xFF,0xA0,0xA0);

  // images
  private final static Image IMAGE_DIRECTORY = Widgets.loadImage(Display.getDefault(),"directory.png");

  /** create new text widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @return text widget
   */
  public static Text newText(Composite            composite,
                             String               toolTipText,
                             final WidgetVariable widgetVariable,
                             final Listener       listener
                            )
  {
    final Text text;

    text = Widgets.newText(composite,SWT.LEFT|SWT.BORDER);
    text.setToolTipText(toolTipText);
    text.setText((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString());
    text.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String s = widget.getText();
        if (((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString()).equals(s)) color = null;

        widget.setBackground(color);
      }
    });
    text.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;

        String string = widget.getText();

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setBackground(null);
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
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

        String string = widget.getText();

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setBackground(null);
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(text,widgetVariable)
        {
          @Override
          public void modified(Text text, WidgetVariable variable)
          {
            text.setText(listener.getString(widgetVariable));
          }
        }
      : new WidgetModifyListener(text,widgetVariable);
    Widgets.addModifyListener(widgetModifiedListener);
    text.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    if (listener != null)
    {
      text.setText(listener.getString(widgetVariable));
    }
    else
    {
      text.setText(widgetVariable.getString());
    }

    return text;
  }

  /** create new text widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return text widget
   */
  public static Text newText(Composite      composite,
                             String         toolTipText,
                             WidgetVariable widgetVariable
                            )
  {
    return newText(composite,toolTipText,widgetVariable,(Listener)null);
  }

  /** create new number widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @param min,max min/max value
   * @return number widget
   */
  public static Spinner newNumber(Composite            composite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  final Listener       listener,
                                  int                  min,
                                  int                  max
                                 )
  {
    final Spinner spinner;

    spinner = Widgets.newSpinner(composite);
    spinner.setToolTipText(toolTipText);
    spinner.setMinimum(min);
    spinner.setMaximum(max);
    spinner.setSelection((listener != null) ? listener.getInt(widgetVariable) : widgetVariable.getInteger());

    spinner.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Spinner widget = (Spinner)modifyEvent.widget;
        int     n      = widget.getSelection();

        Color color = COLOR_MODIFIED;
        if (listener != null)
        {
          if (listener.getInt(widgetVariable) == n) color = null;
        }
        else
        {
          if (widgetVariable.getInteger() == n) color = null;
        }

        widget.setBackground(color);
        widget.setData("showedErrorDialog",false);
      }
    });
    spinner.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Spinner widget = (Spinner)selectionEvent.widget;
        int     n      = widget.getSelection();

        if (listener != null)
        {
          listener.setInt(widgetVariable,n);
        }
        else
        {
          widgetVariable.set(n);
        }

        widget.setBackground(null);
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Spinner widget = (Spinner)selectionEvent.widget;
        int     n      = widget.getSelection();

        if (listener != null)
        {
          listener.setInt(widgetVariable,n);
        }
        else
        {
          widgetVariable.set(n);
        }

        widget.setBackground(null);
      }
    });
    spinner.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
        Spinner widget = (Spinner)focusEvent.widget;
        widget.setData("showedErrorDialog",false);
      }
      public void focusLost(FocusEvent focusEvent)
      {
        Spinner widget = (Spinner)focusEvent.widget;
        int     n      = widget.getSelection();

        if (listener != null)
        {
          listener.setInt(widgetVariable,n);
        }
        else
        {
          widgetVariable.set(n);
        }

        widget.setBackground(null);
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(spinner,widgetVariable)
        {
          @Override
          public void modified(Spinner spinner, WidgetVariable variable)
          {
            spinner.setSelection(listener.getInt(widgetVariable));
          }
        }
      : new WidgetModifyListener(spinner,widgetVariable);
    Widgets.addModifyListener(widgetModifiedListener);
    spinner.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    if (listener != null)
    {
      spinner.setSelection(listener.getInt(widgetVariable));
    }
    else
    {
      spinner.setSelection(widgetVariable.getInteger());
    }

    return spinner;
  }

  /** create new number widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param min,max min/max value
   * @return number widget
   */
  public static Spinner newNumber(Composite      composite,
                                  String         toolTipText,
                                  WidgetVariable widgetVariable,
                                  int            min,
                                  int            max
                                 )
  {
    return newNumber(composite,toolTipText,widgetVariable,(Listener)null,min,max);
  }

  /** create new checkbox widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @param text checkbox text
   * @return button widget
   */
  public static Button newCheckbox(Composite            composite,
                                   String               toolTipText,
                                   final WidgetVariable widgetVariable,
                                   final Listener       listener,
                                   String               text
                                  )
  {
    final Button button;

    button = Widgets.newCheckbox(composite,text);
    button.setToolTipText(toolTipText);

    button.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Button  widget = (Button)selectionEvent.widget;
        boolean b      = widget.getSelection();

        if (listener != null)
        {
          listener.setChecked(widgetVariable,b);
        }
        else
        {
          widgetVariable.set(b);
        }
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button  widget = (Button)selectionEvent.widget;
        boolean b      = widget.getSelection();

        if (listener != null)
        {
          listener.setChecked(widgetVariable,b);
        }
        else
        {
          widgetVariable.set(b);
        }
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(button,widgetVariable)
        {
          @Override
          public void modified(Button button, WidgetVariable variable)
          {
            button.setSelection(listener.getChecked(widgetVariable));
          }
        }
      : new WidgetModifyListener(button,widgetVariable);
    Widgets.addModifyListener(widgetModifiedListener);
    button.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    if (listener != null)
    {
      button.setSelection(listener.getChecked(widgetVariable));
    }
    else
    {
      button.setSelection(widgetVariable.getBoolean());
    }

    return button;
  }

  /** create new checkbox widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param text checkbox text
   * @return button widget
   */
  public static Button newCheckbox(Composite      composite,
                                   String         toolTipText,
                                   WidgetVariable widgetVariable,
                                   String         text
                                  )
  {
    return newCheckbox(composite,toolTipText,widgetVariable,(Listener)null,text);
  }

  /** create new radio widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @param text checkbox text
   * @return button widget
   */
  public static Button newRadio(Composite            composite,
                                String               toolTipText,
                                final WidgetVariable widgetVariable,
                                final Listener       listener,
                                String               text
                               )
  {
    final Button button;

    button = Widgets.newRadio(composite,text);
    button.setToolTipText(toolTipText);

    button.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Button  widget = (Button)selectionEvent.widget;
        boolean b      = widget.getSelection();

        if (listener != null)
        {
          listener.setChecked(widgetVariable,b);
        }
        else
        {
          widgetVariable.set(b);
        }
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button  widget = (Button)selectionEvent.widget;
        boolean b      = widget.getSelection();

        if (listener != null)
        {
          listener.setChecked(widgetVariable,b);
        }
        else
        {
          widgetVariable.set(b);
        }
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(button,widgetVariable)
        {
          @Override
          public void modified(Button button, WidgetVariable variable)
          {
            button.setSelection(listener.getChecked(widgetVariable));
          }
        }
      : new WidgetModifyListener(button,widgetVariable);
    Widgets.addModifyListener(widgetModifiedListener);
    button.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    if (listener != null)
    {
      button.setSelection(listener.getChecked(widgetVariable));
    }
    else
    {
      button.setSelection(widgetVariable.getBoolean());
    }

    return button;
  }

  /** create new radio widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param text checkbox text
   * @return button widget
   */
  public static Button newRadio(Composite      composite,
                                String         toolTipText,
                                WidgetVariable widgetVariable,
                                String         text
                               )
  {
    return newRadio(composite,toolTipText,widgetVariable,(Listener)null,text);
  }

  /** get size
   * @param items items array (string, value)
   * @param string string
   * @param defaultItemIndex item to use on parse error or -1
   * @return size
   */
  private static long getSize(Object[] items, String string, int defaultItemIndex)
  {
    long n = 0;
    int  i;

    assert (items.length % 2) == 0;

    // find matching item string
    i = 0;
    while (i < items.length/2)
    {
      if (string.equals((String)items[i*2+0]))
      {
        n = (Long)items[i*2+1];
        break;
      }
      i++;
    }

    // find matching item value
    if (i >= items.length/2)
    {
      try
      {
        n = Units.parseByteSize(string);
      }
      catch (NumberFormatException exception)
      {
        if (defaultItemIndex >= 0)
        {
          n = (Long)items[defaultItemIndex*2+1];
        }
        else
        {
          throw exception;
        }
      }
    }

    return n;
  }

  /** get size
   * @param items items array (string, value)
   * @param string string
   * @return size
   */
  private static long getSize(Object[] items, String string)
  {
    return getSize(items,string,-1);
  }

  /** get size string
   * @param items items array (string, value)
   * @param string string
   * @param defaultItemIndex item to use on parse error or -1
   * @return size string
   */
  private static String getSizeString(Object[] items, String string, int defaultItemIndex)
  {
    int i;

    assert (items.length % 2) == 0;

    // find matching item string
    i = 0;
    while (i < items.length/2)
    {
      if (string.equals(items[i*2+0]))
      {
        break;
      }
      i++;
    }

    // find matching item value
    if (i >= items.length/2)
    {
      try
      {
        long n = Units.parseByteSize(string);
        i = 0;
        while (i < items.length/2)
        {
          if (n == (Long)(items[i*2+1]))
          {
            string = (String)items[i*2+0];
            break;
          }
          i++;
        }
        if (i >= items.length/2)
        {
          string = Units.getSize(n)+" "+Units.getUnit(n);
        }
      }
      catch (NumberFormatException exception)
      {
        if (defaultItemIndex >= 0)
        {
          string = (String)items[defaultItemIndex*2+0];
        }
        else
        {
          throw exception;
        }
      }
    }

    return string;
  }

  /** get size string
   * @param items items array (string, value)
   * @param string string
   * @return size string
   */
  private static String getSizeString(Object[] items, String string)
  {
    return getSizeString(items,string,-1);
  }

  /** create new byte size widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable or null
   * @param listener listener or null
   * @param values combo values
   * @return number widget
   */
  public static Combo newByteSize(Composite            composite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  final Listener       listener,
                                  final Object[]       items
                                 )
  {
    final Shell  shell = composite.getShell();
    final String values[];
    final Combo  combo;

    assert (items.length % 2) == 0;

    // get values
    values = new String[items.length/2];
    for (int i = 0; i < items.length/2; i++)
    {
      values[i] = (String)items[i*2+0];
    }

    // create combo widget
    combo = Widgets.newCombo(composite);
    combo.setToolTipText(toolTipText);
    combo.setItems(values);
    combo.setData("showedErrorDialog",false);

    // listener
    if (widgetVariable != null)
    {
      combo.addModifyListener(new ModifyListener()
      {
        public void modifyText(ModifyEvent modifyEvent)
        {
          Combo widget = (Combo)modifyEvent.widget;

          Color color = COLOR_MODIFIED;
          try
          {
            long  n0 = getSize(items,widget.getText());
            long  n1;

            if (listener != null)
            {
              n1 = getSize(items,listener.getString(widgetVariable),0);
            }
            else
            {
              n1 = getSize(items,widgetVariable.getString(),0);
            }
            if (n0 == n1) color = null;
          }
          catch (NumberFormatException exception)
          {
            // ignored
          }

          widget.setBackground(color);
          widget.setData("showedErrorDialog",false);
        }
      });
    }
    combo.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;

        String string = widget.getText();
//        if (!string.isEmpty())
        {
          long n = 0;

          try
          {
            string = getSizeString(items,string);
            n      = getSize(items,string);
          }
          catch (NumberFormatException exception)
          {
            if (!(Boolean)widget.getData("showedErrorDialog"))
            {
              widget.setData("showedErrorDialog",true);
              Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
              widget.forceFocus();
            }
          }

          if (widgetVariable != null)
          {
            if (listener != null)
            {
              listener.setString(widgetVariable,Units.formatSize(n));
            }
            else
            {
              widgetVariable.set(Units.formatSize(n));
            }
          }

          widget.setText(string);
          widget.setBackground(null);
        }
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;

        String string = widget.getText();
//        if (!string.isEmpty())
        {
          long n = 0;

          try
          {
            string = getSizeString(items,string);
            n      = getSize(items,string);
          }
          catch (NumberFormatException exception)
          {
            if (!(Boolean)widget.getData("showedErrorDialog"))
            {
              widget.setData("showedErrorDialog",true);
              Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
              widget.forceFocus();
            }
          }

          if (widgetVariable != null)
          {
            if (listener != null)
            {
              listener.setString(widgetVariable,Units.formatSize(n));
            }
            else
            {
              widgetVariable.set(Units.formatSize(n));
            }
          }

          widget.setText(string);
          widget.setBackground(null);
        }
      }
    });
    combo.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
        Combo widget = (Combo)focusEvent.widget;
        widget.setData("showedErrorDialog",false);
      }
      public void focusLost(FocusEvent focusEvent)
      {
        Combo  widget = (Combo)focusEvent.widget;
        String string = widget.getText();

        if (!string.isEmpty())
        {
          long n = 0;

          try
          {
            string = getSizeString(items,string);
            n      = getSize(items,string);
          }
          catch (NumberFormatException exception)
          {
            if (!(Boolean)widget.getData("showedErrorDialog"))
            {
              widget.setData("showedErrorDialog",true);
              Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
              widget.forceFocus();
            }
          }

          if (widgetVariable != null)
          {
            if (listener != null)
            {
              listener.setString(widgetVariable,Units.formatSize(n));
            }
            else
            {
              widgetVariable.set(Units.formatSize(n));
            }
          }

          widget.setText(string);
          widget.setBackground(null);
        }
      }
    });

    if (widgetVariable != null)
    {
      final WidgetModifyListener widgetModifiedListener = (listener != null)
        ? new WidgetModifyListener(combo,widgetVariable)
          {
            @Override
            public void modified(Combo combo, WidgetVariable variable)
            {
              combo.setText(getSizeString(items,listener.getString(widgetVariable),0));
            }
          }
        : new WidgetModifyListener(combo,widgetVariable)
          {
            @Override
            public String getString(WidgetVariable variable)
            {
              return getSizeString(items,variable.getString(),0);
            }
          };
      Widgets.addModifyListener(widgetModifiedListener);
      combo.addDisposeListener(new DisposeListener()
      {
        public void widgetDisposed(DisposeEvent disposedEvent)
        {
          Widgets.removeModifyListener(widgetModifiedListener);
        }
      });
    }

    // set value
    if (widgetVariable != null)
    {
      String string;
      if (listener != null)
      {
        string = listener.getString(widgetVariable);
      }
      else
      {
        string = widgetVariable.getString();
      }
      combo.setText(getSizeString(items,string,0));
    }

    return combo;
  }

  /** create new byte size widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values
   * @return number widget
   */
  public static Combo newByteSize(Composite      composite,
                                  String         toolTipText,
                                  WidgetVariable widgetVariable,
                                  Object[]       items
                                 )
  {
    return newByteSize(composite,toolTipText,widgetVariable,(Listener)null,items);
  }

  /** create new byte size widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param listener listener or null
   * @param values combo values
   * @return number widgets
   */
  public static Combo newByteSize(Composite      composite,
                                  String         toolTipText,
                                  final Listener listener,
                                  final Object[] items
                                 )
  {
    return newByteSize(composite,toolTipText,(WidgetVariable)null,listener,items);
  }

  /** create new byte size widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param values combo values
   * @return number widget
   */
  public static Combo newByteSize(Composite composite,
                                  String    toolTipText,
                                  Object[]  items
                                 )
  {
    return newByteSize(composite,toolTipText,(Listener)null,items);
  }
  /** create new time widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @param values combo values [text,value]
   * @return number widget
   */
  public static Combo newTime(Composite            composite,
                              String               toolTipText,
                              final WidgetVariable widgetVariable,
                              String[]             values,
                              final Listener       listener
                             )
  {
    final Shell shell = composite.getShell();
    final Combo combo;

    combo = Widgets.newCombo(composite);
    combo.setToolTipText(toolTipText);
    combo.setItems(values);
    combo.setData("showedErrorDialog",false);

    combo.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Combo  widget = (Combo)modifyEvent.widget;
        String string = widget.getText();

        Color color = COLOR_MODIFIED;
        try
        {
          if (!string.isEmpty())
          {
            long n = Units.parseLocalizedTime(string);

            if (listener != null)
            {
              string = listener.getString(widgetVariable);
              if (!string.isEmpty())
              {
                if (Units.parseTime(string) == n) color = null;
              }
              else
              {
                color = null;
              }
            }
            else
            {
              string = widgetVariable.getString();
              if (!string.isEmpty())
              {
                if (Units.parseTime(string) == n) color = null;
              }
              else
              {
                color = null;
              }
            }
          }
          else
          {
            color = null;
          }
        }
        catch (NumberFormatException exception1)
        {
          // ignored
        }

        widget.setBackground(color);
        widget.setData("showedErrorDialog",false);
      }
    });
    combo.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;

        long   n      = 0;
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            n      = Units.parseLocalizedTime(string);
            string = Units.formatTime(n);
          }
        }
        catch (NumberFormatException exception)
        {
          if (!(Boolean)widget.getData("showedErrorDialog"))
          {
            widget.setData("showedErrorDialog",true);
            Dialogs.error(shell,BARControl.tr("''{0}'' is not valid time!\n\nEnter a time in the format ''n<{1}|{2}|{3}|{4}|{5}>''",
                                              string,
                                              BARControl.tr("week"),
                                              BARControl.tr("day"),
                                              BARControl.tr("h"),
                                              BARControl.tr("min"),
                                              BARControl.tr("s")
                                             )
                         );
          }
          widget.forceFocus();
          return;
        }

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }
        widget.setText(Units.formatLocalizedTime(n));
        widget.setBackground(null);
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;

        long   n      = 0;
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            n      = Units.parseLocalizedTime(string);
            string = Units.formatTime(n);
          }
        }
        catch (NumberFormatException exception)
        {
          if (!(Boolean)widget.getData("showedErrorDialog"))
          {
            widget.setData("showedErrorDialog",true);
            Dialogs.error(shell,BARControl.tr("''{0}'' is not valid time!\n\nEnter a time in the format ''n<{1}|{2}|{3}|{4}|{5}>''",
                                              string,
                                              BARControl.tr("weeks"),
                                              BARControl.tr("days"),
                                              BARControl.tr("h"),
                                              BARControl.tr("mins"),
                                              BARControl.tr("s")
                                             )
                         );
          }
          widget.forceFocus();
          return;
        }

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setText(Units.formatLocalizedTime(n));
        widget.setBackground(null);
      }
    });
    combo.addFocusListener(new FocusListener()
    {
      public void focusGained(FocusEvent focusEvent)
      {
        Combo widget = (Combo)focusEvent.widget;
        widget.setData("showedErrorDialog",false);
      }
      public void focusLost(FocusEvent focusEvent)
      {
        Combo  widget = (Combo)focusEvent.widget;

        long   n      = 0;
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            n      = Units.parseLocalizedTime(string);
            string = Units.formatTime(n);
          }
        }
        catch (NumberFormatException exception)
        {
          if (!(Boolean)widget.getData("showedErrorDialog"))
          {
            widget.setData("showedErrorDialog",true);
            Dialogs.error(shell,BARControl.tr("''{0}'' is not valid time!\n\nEnter a time in the format ''n<{1}|{2}|{3}|{4}|{5}>''",
                                              string,
                                              BARControl.tr("weeks"),
                                              BARControl.tr("days"),
                                              BARControl.tr("h"),
                                              BARControl.tr("mins"),
                                              BARControl.tr("s")
                                             )
                         );
          }
          widget.forceFocus();
          return;
        }

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setText(Units.formatLocalizedTime(n));
        widget.setBackground(null);
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(combo,widgetVariable)
        {
          @Override
          public void modified(Combo combo, WidgetVariable variable)
          {
            long n = 0;
            try
            {
              n = Units.parseLocalizedTime(listener.getString(widgetVariable));
            }
            catch (NumberFormatException exception1)
            {
              try
              {
                n = Units.parseTime(listener.getString(widgetVariable));
              }
              catch (NumberFormatException exception2)
              {
                // ignored
              }
            }
            combo.setText(Units.formatLocalizedTime(n));
          }
        }
      : new WidgetModifyListener(combo,widgetVariable)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            long n = 0;
            try
            {
              n = Units.parseLocalizedTime(variable.getString());
            }
            catch (NumberFormatException exception1)
            {
              try
              {
                n = Units.parseTime(variable.getString());
              }
              catch (NumberFormatException exception2)
              {
                // ignored
              }
            }
            return Units.formatLocalizedTime(n);
          }
        };
    Widgets.addModifyListener(widgetModifiedListener);
    combo.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    long n = 0;
    if (listener != null)
    {
      try
      {
        n = Units.parseLocalizedTime(listener.getString(widgetVariable));
      }
      catch (NumberFormatException exception1)
      {
        try
        {
          n = Units.parseTime(listener.getString(widgetVariable));
        }
        catch (NumberFormatException exception2)
        {
          // ignored
        }
      }
    }
    else
    {
      try
      {
        n = Units.parseLocalizedTime(widgetVariable.getString());
      }
      catch (NumberFormatException exception1)
      {
        try
        {
          n = Units.parseTime(widgetVariable.getString());
        }
        catch (NumberFormatException exception2)
        {
          // ignored
        }
      }
    }
    combo.setText(Units.formatLocalizedTime(n));

    return combo;
  }

  /** create new time widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values
   * @return number widget
   */
  public static Combo newTime(Composite      composite,
                              String         toolTipText,
                              WidgetVariable widgetVariable,
                              String[]       values
                             )
  {
    return newTime(composite,toolTipText,widgetVariable,values,(Listener)null);
  }

  /** create new path name widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @return composite
   */
  public static Composite newDirectory(Composite            composite,
                                       String               toolTipText,
                                       final WidgetVariable widgetVariable,
                                       final Listener       listener
                                      )
  {
    final Shell shell = composite.getShell();
    Composite   subComposite;
    final Text  text;
    Button      button;

    subComposite = Widgets.newComposite(composite,SWT.NONE);
    subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
    Widgets.layout(subComposite,0,0,TableLayoutData.WE);
    {
      text = Widgets.newText(subComposite,SWT.LEFT|SWT.BORDER);
      text.setToolTipText(toolTipText);
      text.setText(widgetVariable.getString());
      Widgets.layout(text,0,0,TableLayoutData.WE);
      text.addModifyListener(new ModifyListener()
      {
        public void modifyText(ModifyEvent modifyEvent)
        {
          Text   widget = (Text)modifyEvent.widget;
          String string = widget.getText();

          Color color  = COLOR_MODIFIED;
          if (((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString()).equals(string)) color = null;

          widget.setBackground(color);
        }
      });
      text.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          String string = widget.getText();

          widgetVariable.set(string);
          widget.setBackground(null);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
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

          String string = widget.getText();

          widgetVariable.set(string);
          widget.setBackground(null);
        }
      });

      final WidgetModifyListener widgetModifiedListener = new WidgetModifyListener(text,widgetVariable);
      Widgets.addModifyListener(widgetModifiedListener);
      text.addDisposeListener(new DisposeListener()
      {
        public void widgetDisposed(DisposeEvent disposedEvent)
        {
          Widgets.removeModifyListener(widgetModifiedListener);
        }
      });

      button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
      button.setToolTipText(BARControl.tr("Select path name. Ctrl+Click to select local directory."));
      Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String pathName;
          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.DIRECTORY,
                                    BARControl.tr("Select path"),
                                    text.getText(),
                                    BARServer.remoteListDirectory
                                   );
          }
          else
          {
            pathName = Dialogs.directory(shell,
                                         BARControl.tr("Select path"),
                                         text.getText()
                                        );
          }
          if (pathName != null)
          {
            text.setText(pathName);
          }
        }
      });
    }

    return subComposite;
  }

  /** create new path name widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return composite
   */
  public static Composite newDirectory(Composite            composite,
                                       String               toolTipText,
                                       final WidgetVariable widgetVariable
                                      )
  {
    return newDirectory(composite,toolTipText,widgetVariable,(Listener)null);
  }

  /** create new file name widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return composite
   */
  public static Composite newFile(Composite            composite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  final Listener       listener,
                                  final String[]       fileExtensions,
                                  final String         defaultFileExtension
                                 )
  {
    final Shell shell = composite.getShell();
    Composite   subComposite;
    final Text  text;
    Button      button;

    subComposite = Widgets.newComposite(composite,SWT.NONE);
    subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
    Widgets.layout(subComposite,0,0,TableLayoutData.WE);
    {
      text = Widgets.newText(subComposite,SWT.LEFT|SWT.BORDER);
      text.setToolTipText(toolTipText);
      text.setText((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString());
      Widgets.layout(text,0,0,TableLayoutData.WE);
      text.addModifyListener(new ModifyListener()
      {
        public void modifyText(ModifyEvent modifyEvent)
        {
          Text   widget = (Text)modifyEvent.widget;
          String string = widget.getText();

          Color color  = COLOR_MODIFIED;
          if (((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString()).equals(string)) color = null;

          widget.setBackground(color);
        }
      });
      text.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          String string = widget.getText();
          if (listener != null)
          {
            listener.setString(widgetVariable,string);
          }
          else
          {
            widgetVariable.set(string);
          }

          widget.setBackground(null);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
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

          String string = widget.getText();
          if (listener != null)
          {
            listener.setString(widgetVariable,string);
          }
          else
          {
            widgetVariable.set(string);
          }

          widget.setBackground(null);
        }
      });

      final WidgetModifyListener widgetModifiedListener = (listener != null)
        ? new WidgetModifyListener(text,widgetVariable)
          {
            @Override
            public void modified(Text text, WidgetVariable variable)
            {
              text.setText(listener.getString(widgetVariable));
            }
          }
        : new WidgetModifyListener(text,widgetVariable);
      Widgets.addModifyListener(widgetModifiedListener);
      text.addDisposeListener(new DisposeListener()
      {
        public void widgetDisposed(DisposeEvent disposedEvent)
        {
          Widgets.removeModifyListener(widgetModifiedListener);
        }
      });

      button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
      button.setToolTipText(BARControl.tr("Select file name. Ctrl+Click to select local file."));
      Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String fileName;
          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            fileName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select file"),
                                    text.getText(),
                                    fileExtensions,
                                    defaultFileExtension,
                                    BARServer.remoteListDirectory
                                   );
          }
          else
          {
            fileName = Dialogs.fileOpen(shell,
                                        BARControl.tr("Select file"),
                                        text.getText(),
                                        fileExtensions,
                                        defaultFileExtension
                                       );
          }
          if (fileName != null)
          {
            text.setText(fileName);
          }
        }
      });
    }

    return subComposite;
  }

  /** create new file name widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return composite
   */
  public static Composite newFile(Composite            composite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  final String[]       fileExtensions,
                                  final String         defaultFileExtension
                                 )
  {
    return newFile(composite,
                   toolTipText,
                   widgetVariable,
                   (Listener)null,
                   fileExtensions,
                   defaultFileExtension
                  );
  }

  /** create new password widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @return text widget
   */
  public static Text newPassword(Composite            composite,
                                 String               toolTipText,
                                 final WidgetVariable widgetVariable,
                                 final Listener       listener
                                )
  {
    final Text text;

    text = Widgets.newText(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
    text.setToolTipText(toolTipText);
    text.setText((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString());
    text.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String s = widget.getText();
        if (((listener != null) ? listener.getString(widgetVariable) : widgetVariable.getString()).equals(s)) color = null;

        widget.setBackground(color);
      }
    });
    text.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;

        String string = widget.getText();

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setBackground(null);
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
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

        String string = widget.getText();

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setBackground(null);
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(text,widgetVariable)
        {
          @Override
          public void modified(Text text, WidgetVariable variable)
          {
            text.setText(listener.getString(widgetVariable));
          }
        }
      : new WidgetModifyListener(text,widgetVariable);
    Widgets.addModifyListener(widgetModifiedListener);
    text.addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposedEvent)
      {
        Widgets.removeModifyListener(widgetModifiedListener);
      }
    });

    if (listener != null)
    {
      text.setText(listener.getString(widgetVariable));
    }
    else
    {
      text.setText(widgetVariable.getString());
    }

    return text;
  }

  /** create new password widget
   * @param composite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return text widget
   */
  public static Text newPassword(Composite      composite,
                                 String         toolTipText,
                                 WidgetVariable widgetVariable
                                )
  {
    return newPassword(composite,toolTipText,widgetVariable,(Listener)null);
  }

  /** File super widget
   */
  static class File extends Composite
  {
    static enum WidgetTypes
    {
      MAX_STORAGE_SIZE,
      ARCHIVE_FILE_MODE
    };

    Combo maxStorageSize;
    Combo archiveFileMode;

    /** create file super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param maxStorageSizeVariable max. storage size variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public File(final Composite      composite,
                EnumSet<WidgetTypes> widgets,
                final WidgetVariable maxStorageSizeVariable,
                final WidgetVariable archiveFileModeVariable
               )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(0.0,0.0));

      int       row;
      Composite subComposite,subSubComposite;
      Label     label;
      Combo     combo;

      row = 0;

      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,0.0));
      Widgets.layout(subComposite,0,0,TableLayoutData.WE);
      {
        if (widgets.contains(WidgetTypes.MAX_STORAGE_SIZE))
        {
          subSubComposite = Widgets.newComposite(subComposite,SWT.NONE);
          subSubComposite.setLayout(new TableLayout(1.0,0.0));
          Widgets.layout(subSubComposite,row,0,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subSubComposite,BARControl.tr("Max. storage size")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            maxStorageSize = BARWidgets.newByteSize(subSubComposite,
                                                    BARControl.tr("Storage size limit."),
                                                    maxStorageSizeVariable,
                                                    new Object[]{BARControl.tr("unlimited"), 0L,
                                                                 Units.formatSize(  1*Units.G),  1*Units.G,
                                                                 Units.formatSize(  2*Units.G),  2*Units.G,
                                                                 Units.formatSize(  4*Units.G),  4*Units.G,
                                                                 Units.formatSize(  8*Units.G),  8*Units.G,
                                                                 Units.formatSize( 64*Units.G), 64*Units.G,
                                                                 Units.formatSize(128*Units.G),128*Units.G,
                                                                 Units.formatSize(512*Units.G),512*Units.G,
                                                                 Units.formatSize(  1*Units.T),  1*Units.T,
                                                                 Units.formatSize(  2*Units.T),  2*Units.T,
                                                                 Units.formatSize(  4*Units.T),  4*Units.T,
                                                                 Units.formatSize(  8*Units.T),  8*Units.T
                                                                }
                                                   );
            maxStorageSize.setData("showedErrorDialog",false);
            Widgets.layout(maxStorageSize,0,1,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
/*
            maxStorageSize.addModifyListener(new ModifyListener()
            {
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Combo  widget = (Combo)modifyEvent.widget;
                String string = widget.getText();

                if (!string.isEmpty())
                {
                  try
                  {
                    long n = Units.parseByteSize(string);
                    if (maxStorageSizeVariable != null)
                    {
                      Color color = COLOR_MODIFIED;
                      if (maxStorageSizeVariable.getLong() == n) color = null;
                      widget.setBackground(color);
                      widget.setData("showedErrorDialog",false);
                    }
                  }
                  catch (NumberFormatException exception)
                  {
                    // ignored
                  }
                }
              }
            });
            maxStorageSize.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();

                if (!string.isEmpty())
                {
                  try
                  {
                    long n = Units.parseByteSize(string);
                    if (maxStorageSizeVariable != null)
                    {
                      maxStorageSizeVariable.set(n);
                      widget.setText(Units.formatByteSize(n));
                      widget.setBackground(null);
                    }
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(composite.getShell(),
                                    BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",
                                                  string
                                                 )
                                   );
                      widget.forceFocus();
                    }
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();

                if (!string.isEmpty())
                {
                  try
                  {
                    long n = Units.parseByteSize(string);
                    if (maxStorageSizeVariable != null)
                    {
                      maxStorageSizeVariable.set(n);
                      widget.setText(Units.formatByteSize(n));
                      widget.setBackground(null);
                    }
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(composite.getShell(),
                                    BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",
                                                  string
                                                 )
                                   );
                      widget.forceFocus();
                    }
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              }
            });
            maxStorageSize.addFocusListener(new FocusListener()
            {
              @Override
              public void focusGained(FocusEvent focusEvent)
              {
                Combo widget = (Combo)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              @Override
              public void focusLost(FocusEvent focusEvent)
              {
                Combo  widget = (Combo)focusEvent.widget;
                String string = widget.getText();

                if (!string.isEmpty())
                {
                  try
                  {
                    long n = Units.parseByteSize(string);
                    if (maxStorageSizeVariable != null)
                    {
                      maxStorageSizeVariable.set(n);
                      widget.setText(Units.formatByteSize(n));
                      widget.setBackground(null);
                    }
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(composite.getShell(),
                                    BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",
                                                  string
                                                 )
                                   );
                      widget.forceFocus();
                    }
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              }
            });
            if (maxStorageSizeVariable != null)
            {
              Widgets.addModifyListener(new WidgetModifyListener(maxStorageSize,maxStorageSizeVariable)
              {
                @Override
                public void modified(Combo combo, WidgetVariable variable)
                {
                  combo.setText(Units.formatByteSize(variable.getLong()));
                }
              });
            }
*/
          }
          row++;
        }

        if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
        {
          subSubComposite = Widgets.newComposite(subComposite,SWT.NONE);
          subSubComposite.setLayout(new TableLayout(1.0,0.0));
          Widgets.layout(subSubComposite,row,0,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subSubComposite,BARControl.tr("Archive file mode")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            archiveFileMode = Widgets.newOptionMenu(subSubComposite);
            archiveFileMode.setToolTipText(BARControl.tr("If set to ''rename'' then the archive is renamed if it already exists.\nIf set to ''append'' then data is appended to the existing archive files.\nIf set to ''overwrite'' then existing archive files are overwritten.\nOtherwise stop with an error."));
            Widgets.setComboItems(archiveFileMode,new Object[]{BARControl.tr("stop if exists"  ),"stop",
                                                               BARControl.tr("rename if exists"),"rename",
                                                               BARControl.tr("append"          ),"append",
                                                               BARControl.tr("overwrite"       ),"overwrite"
                                                              }
                                 );
            Widgets.layout(archiveFileMode,0,1,TableLayoutData.W);
            archiveFileMode.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = Widgets.getSelectedComboItem(widget,"stop");

                if (archiveFileModeVariable != null)
                {
                  archiveFileModeVariable.set(string);
                }
              }
            });
            if (archiveFileModeVariable != null)
            {
              Widgets.addModifyListener(new WidgetModifyListener(archiveFileMode,archiveFileModeVariable));
            }
            row++;
          }
        }
      }

      setVisible(false);
    }

    /** create file super widget
     * @param composite parent composite
     * @param maxStorageSizeVariable max. storage size variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public File(final Composite      composite,
                final WidgetVariable maxStorageSizeVariable,
                final WidgetVariable archiveFileModeVariable
               )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           maxStorageSizeVariable,
           archiveFileModeVariable
          );
    }

    /** create file super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public File(Composite            composite,
                EnumSet<WidgetTypes> widgets
               )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create device super widget
     * @param composite parent composite
     */
    public File(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class),
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }
  };

  /** FTP super widget
   */
  static class FTP extends Composite
  {
    enum WidgetTypes
    {
      ARCHIVE_FILE_MODE
    };

    Text  hostName;
    Text  loginName;
    Text  loginPassword;
    Combo archiveFileMode;

    /** create FTP super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param hostNameVariable host name variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public FTP(final Composite      composite,
               EnumSet<WidgetTypes> widgets,
               final WidgetVariable hostNameVariable,
               final WidgetVariable loginNameVariable,
               final WidgetVariable loginPasswordVariable,
               final WidgetVariable archiveFileModeVariable
              )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(1.0,new double[]{0.0,1.0}));

      int       row;
      Composite subComposite;
      Label     label;

      row = 0;

      label = Widgets.newLabel(this,BARControl.tr("Server")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,new double[]{1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        hostName = Widgets.newText(subComposite);
        hostName.setToolTipText(BARControl.tr("FTP server name."));
        Widgets.layout(hostName,0,0,TableLayoutData.WE);
        hostName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (hostNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (hostNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        hostName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text   widget = (Text)selectionEvent.widget;
            String string = widget.getText();

            if (hostNameVariable != null)
            {
              hostNameVariable.set(string);
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        hostName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (hostNameVariable != null)
            {
              hostNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (hostNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(hostName,hostNameVariable));
        }
        row++;
      }

      label = Widgets.newLabel(this,BARControl.tr("Login")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        loginName = Widgets.newText(subComposite);
        loginName.setToolTipText(BARControl.tr("FTP server user login name. Leave it empty to use the default name from the configuration file."));
        Widgets.layout(loginName,0,0,TableLayoutData.WE);
        loginName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginName,loginNameVariable));
        }

        label = Widgets.newLabel(subComposite,BARControl.tr("Password")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        loginPassword = Widgets.newPassword(subComposite);
        loginPassword.setToolTipText(BARControl.tr("FTP server login password. Leave it empty to use the default password from the configuration file."));
        Widgets.layout(loginPassword,0,2,TableLayoutData.WE);
        loginPassword.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginPasswordVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginPasswordVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginPassword.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginPassword.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginPasswordVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginPassword,loginPasswordVariable));
        }
        row++;
      }

/*
      label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      composite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(composite,1,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(composite,BARControl.tr("unlimited"));
        Widgets.layout(button,0,0,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            try
            {
              maxBandWidthFlag.set(false);
              maxBandWidth.set(0);
              BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
            }
            catch (Exception exception)
            {
              // ignored
            }
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
        {
          @Override
          public void modified(Control control, WidgetVariable archivePartSizeFlag)
          {
            ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
            widgetFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
          }
        });

        button = Widgets.newRadio(composite,BARControl.tr("limit to"));
        Widgets.layout(button,0,1,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            archivePartSizeFlag.set(true);
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
        {
          @Override
          public void modified(Control control, WidgetVariable archivePartSizeFlag)
          {
            ((Button)control).setSelection(maxBandWidthFlag.getBoolean());
            widgetFTPMaxBandWidth.setEnabled(maxBandWidthFlag.getBoolean());
          }
        });

        widgetFTPMaxBandWidth = Widgets.newCombo(composite);
        widgetFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
        Widgets.layout(widgetFTPMaxBandWidth,0,2,TableLayoutData.W);
      }
*/

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("Archive file mode")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);

        archiveFileMode = Widgets.newOptionMenu(this);
        archiveFileMode.setToolTipText(BARControl.tr("If set to ''rename'' then the archive is renamed if it already exists.\nIf set to ''append'' then data is appended to the existing archive files.\nIf set to ''overwrite'' then existing archive files are overwritten."));
        Widgets.setComboItems(archiveFileMode,new Object[]{BARControl.tr("stop if exists"  ),"stop",
                                                           BARControl.tr("rename if exists"),"rename",
                                                           BARControl.tr("append"          ),"append",
                                                           BARControl.tr("overwrite"       ),"overwrite"
                                                          }
                             );
        Widgets.layout(archiveFileMode,row,1,TableLayoutData.W);
        archiveFileMode.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo  widget = (Combo)selectionEvent.widget;
            String string = Widgets.getSelectedComboItem(widget,"stop");

            if (archiveFileModeVariable != null)
            {
              archiveFileModeVariable.set(string);
            }
          }
        });
        if (archiveFileModeVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(archiveFileMode,archiveFileModeVariable));
        }
        row++;
      }

      setVisible(false);
    }

    /** create FTP super widget
     * @param composite parent composite
     * @param hostNameVariable host name variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public FTP(final Composite      composite,
               final WidgetVariable hostNameVariable,
               final WidgetVariable loginNameVariable,
               final WidgetVariable loginPasswordVariable,
               final WidgetVariable archiveFileModeVariable
              )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           hostNameVariable,
           loginNameVariable,
           loginPasswordVariable,
           archiveFileModeVariable
          );
    }

    /** create FTP super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public FTP(final Composite      composite,
               EnumSet<WidgetTypes> widgets
              )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create FTP super widget
     * @param composite parent composite
     */
    public FTP(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class)
          );
    }
  };

  /** SCP/SFTP super widget
   */
  static class SFTP extends Composite
  {
    enum WidgetTypes
    {
      PUBLIC_KEY,
      PRIVATE_KEY,
      ARCHIVE_FILE_MODE
    };

    Text    hostName;
    Spinner hostPort;
    Text    loginName;
    Text    loginPassword;
    Text    publicKey,privateKey;
    Combo   archiveFileMode;

    /** create SCP/SFTP super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param hostNameVariable host name variable (can be null)
     * @param hostPortVariable host port variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param publicKeyVariable public key variable (can be null)
     * @param privateKeyVariable private key variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public SFTP(final Composite      composite,
                EnumSet<WidgetTypes> widgets,
                final WidgetVariable hostNameVariable,
                final WidgetVariable hostPortVariable,
                final WidgetVariable loginNameVariable,
                final WidgetVariable loginPasswordVariable,
                final WidgetVariable publicKeyVariable,
                final WidgetVariable privateKeyVariable,
                final WidgetVariable archiveFileModeVariable
               )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));

      int       row;
      Composite subComposite;
      Label     label;
      Button    button;

      row = 0;

      label = Widgets.newLabel(this,BARControl.tr("Server")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,new double[]{1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        hostName = Widgets.newText(subComposite);
        hostName.setToolTipText(BARControl.tr("SCP/SFTP server name."));
        Widgets.layout(hostName,0,0,TableLayoutData.WE);
        hostName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (hostNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (hostNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        hostName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (hostNameVariable != null)
            {
              hostNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        hostName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (hostNameVariable != null)
            {
              hostNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (hostNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(hostName,hostNameVariable));
        }

        label = Widgets.newLabel(subComposite,BARControl.tr("Port")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        hostPort = Widgets.newSpinner(subComposite);
        hostPort.setToolTipText(BARControl.tr("SSH port number. Set to 0 to use default port number from configuration file."));
        hostPort.setMinimum(0);
        hostPort.setMaximum(65535);
        hostPort.setData("showedErrorDialog",false);
        Widgets.layout(hostPort,0,2,TableLayoutData.W,0,0,0,0,80,SWT.DEFAULT);
        hostPort.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Spinner widget = (Spinner)modifyEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (hostPortVariable.getInteger() == n) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        hostPort.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Spinner widget = (Spinner)selectionEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Spinner widget = (Spinner)selectionEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
        });
        hostPort.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Spinner widget = (Spinner)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Spinner widget = (Spinner)focusEvent.widget;

            if (hostPortVariable != null)
            {
              hostPortVariable.set(widget.getSelection());
              widget.setBackground(null);
            }
          }
        });
        if (hostPortVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(hostPort,hostPortVariable));
        }
      }
      row++;

      label = Widgets.newLabel(this,BARControl.tr("Login")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        loginName = Widgets.newText(subComposite);
        loginName.setToolTipText(BARControl.tr("SSH server user login name. Leave it empty to use the default name from the configuration file."));
        Widgets.layout(loginName,0,0,TableLayoutData.WE);
        loginName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginName,loginNameVariable));
        }

        label = Widgets.newLabel(subComposite,BARControl.tr("Password")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        loginPassword = Widgets.newPassword(subComposite);
        loginPassword.setToolTipText(BARControl.tr("SSH server login password. Leave it empty to use the default password from the configuration file."));
        Widgets.layout(loginPassword,0,2,TableLayoutData.WE);
        loginPassword.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginPasswordVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginPasswordVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginPassword.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginPassword.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginPasswordVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginPassword,loginPasswordVariable));
        }
      }
      row++;

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("SSH public key")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          publicKey = Widgets.newText(subComposite);
          publicKey.setToolTipText(BARControl.tr("SSH public key file name. Leave it empty to use the default key file from the configuration file."));
          Widgets.layout(publicKey,0,0,TableLayoutData.WE);
          publicKey.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                Color color = COLOR_MODIFIED;
                if (publicKeyVariable.getString().equals(string)) color = null;
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            }
          });
          publicKey.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                publicKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          publicKey.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;

              widget.setData("showedErrorDialog",false);
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                publicKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
          });
          if (publicKeyVariable != null)
          {
            Widgets.addModifyListener(new WidgetModifyListener(publicKey,publicKeyVariable));
          }

          button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(composite.getShell(),
                                      Dialogs.FileDialogTypes.OPEN,
                                      BARControl.tr("Select SSH public key file"),
                                      publicKey.getText(),
                                      new String[]{BARControl.tr("Public key files"),"*.pub",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                publicKey.setText(fileName);
                if (publicKeyVariable != null)
                {
                  publicKeyVariable.set(publicKey.getText());
                  publicKey.setBackground(null);
                }
              }
            }
          });
        }
        row++;
      }

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("SSH private key")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          privateKey = Widgets.newText(subComposite);
          privateKey.setToolTipText(BARControl.tr("SSH private key file name. Leave it empty to use the default key file from the configuration file."));
          Widgets.layout(privateKey,0,0,TableLayoutData.WE);
          privateKey.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                Color color = COLOR_MODIFIED;
                if (privateKeyVariable.getString().equals(string)) color = null;
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            }
          });
          privateKey.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                privateKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          privateKey.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;

              widget.setData("showedErrorDialog",false);
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                privateKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
          });
          if (privateKeyVariable != null)
          {
            Widgets.addModifyListener(new WidgetModifyListener(privateKey,privateKeyVariable));
          }

          button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(composite.getShell(),
                                      Dialogs.FileDialogTypes.OPEN,
                                      BARControl.tr("Select SSH private key file"),
                                      privateKey.getText(),
                                      new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                privateKey.setText(fileName);
                if (privateKeyVariable != null)
                {
                  privateKeyVariable.set(privateKey.getText());
                  privateKey.setBackground(null);
                }
              }
            }
          });
        }
        row++;
      }

/*
      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
      label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      composite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(composite,1,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(composite,BARControl.tr("unlimited"));
        Widgets.layout(button,0,0,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            try
            {
              maxBandWidthFlag.set(false);
              maxBandWidth.set(0);
              BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
            }
            catch (Exception exception)
            {
              // ignored
            }
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
        {
          @Override
          public void modified(Control control, WidgetVariable archivePartSizeFlag)
          {
            ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
            widgetFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
          }
        });

        button = Widgets.newRadio(composite,BARControl.tr("limit to"));
        Widgets.layout(button,0,1,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            archivePartSizeFlag.set(true);
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
        {
          @Override
          public void modified(Control control, WidgetVariable archivePartSizeFlag)
          {
            ((Button)control).setSelection(maxBandWidthFlag.getBoolean());
            widgetFTPMaxBandWidth.setEnabled(maxBandWidthFlag.getBoolean());
          }
        });

        widgetFTPMaxBandWidth = Widgets.newCombo(composite);
        widgetFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
        Widgets.layout(widgetFTPMaxBandWidth,0,2,TableLayoutData.W);
      }
*/

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("Archive file mode")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);

        archiveFileMode = Widgets.newOptionMenu(this);
        archiveFileMode.setToolTipText(BARControl.tr("If set to ''rename'' then the archive is renamed if it already exists.\nIf set to ''append'' then data is appended to the existing archive files.\nIf set to ''overwrite'' then existing archive files are overwritten."));
        Widgets.setComboItems(archiveFileMode,new Object[]{BARControl.tr("stop if exists"  ),"stop",
                                                           BARControl.tr("rename if exists"),"rename",
                                                           BARControl.tr("append"          ),"append",
                                                           BARControl.tr("overwrite"       ),"overwrite"
                                                          }
                             );
        Widgets.layout(archiveFileMode,row,1,TableLayoutData.W);
        archiveFileMode.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo  widget = (Combo)selectionEvent.widget;
            String string = Widgets.getSelectedComboItem(widget,"stop");

            if (archiveFileModeVariable != null)
            {
              archiveFileModeVariable.set(string);
            }
          }
        });
        if (archiveFileModeVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(archiveFileMode,archiveFileModeVariable));
        }
        row++;
      }

      setVisible(false);
    }

    /** create device super widget
     * @param composite parent composite
     * @param hostNameVariable host name variable (can be null)
     * @param hostPortVariable host port variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param publicKeyVariable public key variable (can be null)
     * @param privateKeyVariable private key variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public SFTP(final Composite      composite,
                final WidgetVariable hostNameVariable,
                final WidgetVariable hostPortVariable,
                final WidgetVariable loginNameVariable,
                final WidgetVariable loginPasswordVariable,
                final WidgetVariable publicKeyVariable,
                final WidgetVariable privateKeyVariable,
                final WidgetVariable archiveFileModeVariable
               )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           hostNameVariable,
           hostPortVariable,
           loginNameVariable,
           loginPasswordVariable,
           publicKeyVariable,
           privateKeyVariable,
           archiveFileModeVariable
          );
    }

    /** create SCP/SFTP super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public SFTP(Composite            composite,
                EnumSet<WidgetTypes> widgets
               )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create SCP/SFTP super widget
     */
    public SFTP(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class)
          );
    }
  };

  /** WebDAV super widget
   */
  static class WebDAV extends Composite
  {
    enum WidgetTypes
    {
      PUBLIC_KEY,
      PRIVATE_KEY,
      ARCHIVE_FILE_MODE
    };

    Text    hostName;
    Spinner hostPort;
    Text    loginName;
    Text    loginPassword;
    Text    publicKey,privateKey;
    Combo   archiveFileMode;

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param hostNameVariable host name variable (can be null)
     * @param hostPortVariable host port variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param publicKeyVariable public key variable (can be null)
     * @param privateKeyVariable private key variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public WebDAV(final Composite      composite,
                  EnumSet<WidgetTypes> widgets,
                  final WidgetVariable hostNameVariable,
                  final WidgetVariable hostPortVariable,
                  final WidgetVariable loginNameVariable,
                  final WidgetVariable loginPasswordVariable,
                  final WidgetVariable publicKeyVariable,
                  final WidgetVariable privateKeyVariable,
                  final WidgetVariable archiveFileModeVariable
                 )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));

      int       row;
      Composite subComposite;
      Label     label;
      Spinner   spinner;
      Text      text;
      Button    button;
      Combo     combo;

      row = 0;

      label = Widgets.newLabel(this,BARControl.tr("Server")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,new double[]{1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        hostName = Widgets.newText(subComposite);
        hostName.setToolTipText(BARControl.tr("WebDAV server name."));
        Widgets.layout(hostName,0,0,TableLayoutData.WE);
        hostName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (hostNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (hostNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        hostName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text   widget = (Text)selectionEvent.widget;
            String string = widget.getText();

            if (hostNameVariable != null)
            {
              hostNameVariable.set(string);
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        hostName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (hostNameVariable != null)
            {
              hostNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (hostNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(hostName,hostNameVariable));
        }

        label = Widgets.newLabel(subComposite,BARControl.tr("Port")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        hostPort = Widgets.newSpinner(subComposite);
        hostPort.setToolTipText(BARControl.tr("WebDAV port number. Set to 0 to use default port number from configuration file."));
        hostPort.setMinimum(0);
        hostPort.setMaximum(65535);
        hostPort.setData("showedErrorDialog",false);
        Widgets.layout(hostPort,0,2,TableLayoutData.W,0,0,0,0,80,SWT.DEFAULT);
        hostPort.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Spinner widget = (Spinner)modifyEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (hostPortVariable.getInteger() == n) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        hostPort.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Spinner widget = (Spinner)selectionEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Spinner widget = (Spinner)selectionEvent.widget;
            int     n      = widget.getSelection();

            if (hostPortVariable != null)
            {
              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
        });
        hostPort.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Spinner widget = (Spinner)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Spinner widget = (Spinner)focusEvent.widget;

            if (hostPortVariable != null)
            {
              hostPortVariable.set(widget.getSelection());
              widget.setBackground(null);
            }
          }
        });
        if (hostPortVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(hostPort,hostPortVariable));
        }
      }
      row++;

      label = Widgets.newLabel(this,BARControl.tr("Login")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,1.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        loginName = Widgets.newText(subComposite);
        loginName.setToolTipText(BARControl.tr("WebDAV server user login name. Leave it empty to use the default name from the configuration file."));
        Widgets.layout(loginName,0,0,TableLayoutData.WE);
        loginName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginNameVariable != null)
            {
              loginNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginName,loginNameVariable));
        }

        label = Widgets.newLabel(subComposite,BARControl.tr("Password")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        loginPassword = Widgets.newPassword(subComposite);
        loginPassword.setToolTipText(BARControl.tr("WebDAV server login password. Leave it empty to use the default password from the configuration file."));
        Widgets.layout(loginPassword,0,2,TableLayoutData.WE);
        loginPassword.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (loginPasswordVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (loginPasswordVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        loginPassword.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        loginPassword.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (loginPasswordVariable != null)
            {
              loginPasswordVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (loginPasswordVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(loginPassword,loginPasswordVariable));
        }
      }
      row++;

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("WebDAV public key")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          publicKey = Widgets.newText(subComposite);
          publicKey.setToolTipText(BARControl.tr("WebDAV public key file name. Leave it empty to use the default key file from the configuration file."));
          Widgets.layout(publicKey,0,0,TableLayoutData.WE);
          publicKey.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                Color color = COLOR_MODIFIED;
                if (publicKeyVariable.getString().equals(string)) color = null;
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            }
          });
          publicKey.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                publicKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          publicKey.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;

              widget.setData("showedErrorDialog",false);
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              if (publicKeyVariable != null)
              {
                publicKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
          });
          if (publicKeyVariable != null)
          {
            Widgets.addModifyListener(new WidgetModifyListener(publicKey,publicKeyVariable));
          }

          button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(composite.getShell(),
                                      Dialogs.FileDialogTypes.OPEN,
                                      BARControl.tr("Select WebDAV public key file"),
                                      publicKey.getText(),
                                      new String[]{BARControl.tr("Public key files"),"*.pub",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                publicKey.setText(fileName);
                if (publicKeyVariable != null)
                {
                  publicKeyVariable.set(publicKey.getText());
                  publicKey.setBackground(null);
                }
              }
            }
          });
        }
        row++;
      }

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("WebDAV private key")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          privateKey = Widgets.newText(subComposite);
          privateKey.setToolTipText(BARControl.tr("WebDAV private key file name. Leave it empty to use the default key file from the configuration file."));
          Widgets.layout(privateKey,0,0,TableLayoutData.WE);
          privateKey.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                Color color = COLOR_MODIFIED;
                if (privateKeyVariable.getString().equals(string)) color = null;
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            }
          });
          privateKey.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                privateKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          privateKey.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;

              widget.setData("showedErrorDialog",false);
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              if (privateKeyVariable != null)
              {
                privateKeyVariable.set(widget.getText());
                widget.setBackground(null);
              }
            }
          });
          if (privateKeyVariable != null)
          {
            Widgets.addModifyListener(new WidgetModifyListener(privateKey,privateKeyVariable));
          }

          button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(composite.getShell(),
                                      Dialogs.FileDialogTypes.OPEN,
                                      BARControl.tr("Select WebDAV private key file"),
                                      privateKey.getText(),
                                      new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                privateKey.setText(fileName);
                if (privateKeyVariable != null)
                {
                  privateKeyVariable.set(privateKey.getText());
                  privateKey.setBackground(null);
                }
              }
            }
          });
        }
        row++;
      }

// TODO: NYI
/*
      label = Widgets.newLabel(this,BARControl.tr("Max. band width")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      Widgets.layout(subComposite,4,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(subComposite,BARControl.tr("unlimited"));
        Widgets.layout(button,0,0,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

//            if (hostPortVariable != null)
            {
//              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
        });

        button = Widgets.newRadio(subComposite,BARControl.tr("limit to"));
        Widgets.layout(button,0,1,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

//            if (hostPortVariable != null)
            {
//              hostPortVariable.set(n);
              widget.setBackground(null);
            }
          }
        });

        combo = Widgets.newCombo(subComposite);
        combo.setItems(new String[]{"32K","64K","128K","256K","512K"});
        Widgets.layout(combo,0,2,TableLayoutData.W);
      }
*/

      if (widgets.contains(WidgetTypes.ARCHIVE_FILE_MODE))
      {
        label = Widgets.newLabel(this,BARControl.tr("Archive file mode")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,0.0));
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          archiveFileMode = Widgets.newOptionMenu(subComposite);
          archiveFileMode.setToolTipText(BARControl.tr("If set to 'append' then append data to existing archive files.\nIf set to 'overwrite' then overwrite existing files.\nOtherwise stop with an error if archive file exists."));
          Widgets.setComboItems(archiveFileMode,new Object[]{BARControl.tr("stop if exists"),"stop",
                                                             BARControl.tr("rename"        ),"rename",
                                                             BARControl.tr("append"        ),"append",
                                                             BARControl.tr("overwrite"     ),"overwrite",
                                                            }
                               );
          Widgets.layout(archiveFileMode,0,1,TableLayoutData.W);
          archiveFileMode.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = Widgets.getSelectedComboItem(widget,"stop");

              if (archiveFileModeVariable != null)
              {
                archiveFileModeVariable.set(string);
              }
            }
          });
          if (archiveFileModeVariable != null)
          {
            Widgets.addModifyListener(new WidgetModifyListener(archiveFileMode,archiveFileModeVariable));
          }
        }
        row++;
      }

      setVisible(false);
    }

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param hostNameVariable host name variable (can be null)
     * @param hostPortVariable host port variable (can be null)
     * @param loginNameVariable login name variable (can be null)
     * @param loginPasswordVariable login password variable (can be null)
     * @param publicKeyVariable public key variable (can be null)
     * @param privateKeyVariable private key variable (can be null)
     * @param archiveFileModeVariable archive mode variable (can be null)
     */
    public WebDAV(final Composite      composite,
                  final WidgetVariable hostNameVariable,
                  final WidgetVariable hostPortVariable,
                  final WidgetVariable loginNameVariable,
                  final WidgetVariable loginPasswordVariable,
                  final WidgetVariable publicKeyVariable,
                  final WidgetVariable privateKeyVariable,
                  final WidgetVariable archiveFileModeVariable
                 )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           hostNameVariable,
           hostPortVariable,
           loginNameVariable,
           loginPasswordVariable,
           publicKeyVariable,
           privateKeyVariable,
           archiveFileModeVariable
          );
    }

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public WebDAV(final Composite      composite,
                  EnumSet<WidgetTypes> widgets
                 )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create device super widget
     */
    public WebDAV(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class)
          );
    }
  };

  /** Optical super widget
   */
  static class Optical extends Composite
  {
    static enum WidgetTypes
    {
      VOLUME_SIZE,
      ECC,
      BLANK,
      WAIT_FIRST_VOLUME,
      ARCHIVE_PART_SIZE
    };

    // max. size of medium data with ECC [%]
    private final double MAX_MEDIUM_SIZE_ECC = 0.8;

    Text   deviceName;
    Combo  volumeSize;
    Button ecc;
    Button blank;
    Button waitFirstVolume;

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param deviceNameVariable device name variable (can be null)
     * @param volumeSizeVariable volume size variable (can be null)
     * @param eccVariable add error correction codes variable (can be null)
     * @param blankVariable blank medium variable (can be null)
     * @param waitFirstVolumeVariable wait for first medium variable (can be null)
     * @param archivePartSizeFlag archive parts enable variable (can be null)
     * @param archivePartSize archive part size variable (can be null)
     */
    public Optical(final Composite      composite,
                   EnumSet<WidgetTypes> widgets,
                   final WidgetVariable deviceNameVariable,
                   final WidgetVariable volumeSizeVariable,
                   final WidgetVariable eccVariable,
                   final WidgetVariable blankVariable,
                   final WidgetVariable waitFirstVolumeVariable,
                   final WidgetVariable archivePartSizeFlag,
                   final WidgetVariable archivePartSize
                  )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));

      final String CD_DVD_BD_INFO = BARControl.tr("When writing to a CD/DVD/BD with error-correction codes enabled\n"+
                                                  "some free space should be available on medium for error-correction\n"+
                                                  "codes (~20%).\n"+
                                                  "\n"+
                                                  "Good settings may be:\n"+
                                                  "- part size 140M,\tsize 560M,\tmedium 700M\n"+
                                                  "- part size 800M,\tsize 3.2G,\tmedium 4.0G\n"+
                                                  "- part size 1800M,\tsize 7.2G,\tmedium 8.0G\n"+
                                                  "- part size 4G,\t\tsize 20G,\tmedium 25.0G\n"+
                                                  "- part size 10G,\t\tsize 40G,\tmedium 50.0G\n"+
                                                  "- part size 20G,\t\tsize 80G,\tmedium 100.0G"
                                                 );

      int       row;
      Composite subComposite;
      Label     label;
      Spinner   spinner;
      Text      text;
      Button    button;
      Combo     combo;

      row = 0;

      label = Widgets.newLabel(this,BARControl.tr("Device")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        deviceName = Widgets.newText(subComposite);
        deviceName.setToolTipText(BARControl.tr("Device name. Leave it empty to use system default device name."));
        Widgets.layout(deviceName,0,0,TableLayoutData.WE);
        deviceName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (deviceNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (deviceNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        deviceName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (deviceNameVariable != null)
            {
              deviceNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        deviceName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (deviceNameVariable != null)
            {
              deviceNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (deviceNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(deviceName,deviceNameVariable));
        }

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        button.setToolTipText(BARControl.tr("Select remote device. CTRL+click to select local device."));
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String fileName;

            fileName = Dialogs.file(composite.getShell(),
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select device name"),
                                    deviceName.getText(),
                                    new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                },
                                    "*",
                                    ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                      ? BARServer.remoteListDirectory
                                      : BARControl.listDirectory
                                   );
            if (fileName != null)
            {
              deviceName.setText(fileName);
              if (deviceNameVariable != null)
              {
                deviceNameVariable.set(fileName);
              }
            }
          }
        });
      }
      row++;

      if (widgets.contains(WidgetTypes.VOLUME_SIZE))
      {
        label = Widgets.newLabel(this,BARControl.tr("Size")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          volumeSize = BARWidgets.newByteSize(subComposite,
                                              BARControl.tr("Size of medium. You may specify a smaller value than the real physical size to leave some free space for error-correction codes."),
                                              volumeSizeVariable,
                                              new Object[]{Units.formatSize(       425*Units.M ),       425*Units.M,
                                                           Units.formatSize(       430*Units.M ),       430*Units.M,
                                                           Units.formatSize(       470*Units.M ),       470*Units.M,
                                                           Units.formatSize(       520*Units.M ),       520*Units.M,
                                                           Units.formatSize(       560*Units.M ),       560*Units.M,
                                                           Units.formatSize(       650*Units.M ),       650*Units.M,
                                                           Units.formatSize(       660*Units.M ),       660*Units.M,
                                                           Units.formatSize(       720*Units.M ),       720*Units.M,
                                                           Units.formatSize(       850*Units.M ),       850*Units.M,
                                                           Units.formatSize(         2*Units.G ),         2*Units.G,
                                                           Units.formatSize((long)(3.2*Units.G)),(long)(3.2*Units.G),
                                                           Units.formatSize(         4*Units.G ),         4*Units.G,
                                                           Units.formatSize((long)(6.4*Units.G)),(long)(6.4*Units.G),
                                                           Units.formatSize((long)(7.2*Units.G)),(long)(7.2*Units.G),
                                                           Units.formatSize(         8*Units.G ),         8*Units.G,
                                                           Units.formatSize(        20*Units.G ),        20*Units.G,
                                                           Units.formatSize(        25*Units.G ),        25*Units.G,
                                                           Units.formatSize(        40*Units.G ),        40*Units.G,
                                                           Units.formatSize(        50*Units.G ),        50*Units.G,
                                                           Units.formatSize(        80*Units.G ),        80*Units.G,
                                                           Units.formatSize(       100*Units.G ),       100*Units.G
                                                          }
                                             );
          volumeSize.setData("showedErrorDialog",false);
          Widgets.layout(volumeSize,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
        }
        row++;
      }

      if (widgets.contains(WidgetTypes.ECC) || widgets.contains(WidgetTypes.BLANK) || widgets.contains(WidgetTypes.WAIT_FIRST_VOLUME))
      {
        label = Widgets.newLabel(this,BARControl.tr("Options")+":");
        Widgets.layout(label,row,0,TableLayoutData.NW);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          if (widgets.contains(WidgetTypes.ECC))
          {
            ecc = Widgets.newCheckbox(subComposite,BARControl.tr("add error-correction codes"));
            ecc.setToolTipText(BARControl.tr("Add error-correction codes to CD/DVD/BD image (require dvdisaster tool)."));
            Widgets.layout(ecc,0,0,TableLayoutData.W);
            ecc.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                if (eccVariable != null)
                {
                  eccVariable.set(checkedFlag);
                }

                try
                {
                  long n    = Units.parseByteSize(volumeSize.getText());
                  long size = (long)((double)n*MAX_MEDIUM_SIZE_ECC);

                  if (   checkedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((size%archivePartSize.getLong()) > 0)
                      && ((double)(size%archivePartSize.getLong()) < (double)archivePartSize.getLong()*0.5)
                     )
                  {
                    Dialogs.warning(composite.getShell(),CD_DVD_BD_INFO);
                  }
                }
                catch (NumberFormatException exception)
                {
                  // ignored
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            if (eccVariable != null)
            {
              Widgets.addModifyListener(new WidgetModifyListener(ecc,eccVariable));
            }
          }

          if (widgets.contains(WidgetTypes.BLANK))
          {
            blank = Widgets.newCheckbox(subComposite,BARControl.tr("blank medium"));
            blank.setToolTipText(BARControl.tr("Blank medium before writing."));
            Widgets.layout(blank,0,1,TableLayoutData.W);
            blank.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                if (blankVariable != null)
                {
                  blankVariable.set(checkedFlag);
                }
              }
            });
            if (blankVariable != null)
            {
              Widgets.addModifyListener(new WidgetModifyListener(blank,blankVariable));
            }
          }

          if (widgets.contains(WidgetTypes.WAIT_FIRST_VOLUME))
          {
            waitFirstVolume = Widgets.newCheckbox(subComposite,BARControl.tr("wait for first volume"));
            waitFirstVolume.setToolTipText(BARControl.tr("Wait until first volume is loaded."));
            Widgets.layout(waitFirstVolume,1,0,TableLayoutData.W);
            waitFirstVolume.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                if (waitFirstVolumeVariable != null)
                {
                  waitFirstVolumeVariable.set(checkedFlag);
                }
              }
            });
            if (waitFirstVolumeVariable != null)
            {
              Widgets.addModifyListener(new WidgetModifyListener(waitFirstVolume,waitFirstVolumeVariable));
            }
          }
        }
        row++;
      }

      setVisible(false);
    }

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param deviceNameVariable device name variable (can be null)
     * @param volumeSizeVariable volume size variable (can be null)
     * @param eccVariable add error correction codes variable (can be null)
     * @param blankVariable blank medium variable (can be null)
     * @param waitFirstVolumeVariable wait for first medium variable (can be null)
     * @param archivePartSizeFlag archive parts enable variable (can be null)
     * @param archivePartSize archive part size variable (can be null)
     */
    public Optical(final Composite      composite,
                   final WidgetVariable deviceNameVariable,
                   final WidgetVariable volumeSizeVariable,
                   final WidgetVariable eccVariable,
                   final WidgetVariable blankVariable,
                   final WidgetVariable waitFirstVolumeVariable,
                   final WidgetVariable archivePartSizeFlag,
                   final WidgetVariable archivePartSize
                  )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           deviceNameVariable,
           volumeSizeVariable,
           eccVariable,
           blankVariable,
           waitFirstVolumeVariable,
           archivePartSizeFlag,
           archivePartSize
          );
    }

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public Optical(final Composite      composite,
                   EnumSet<WidgetTypes> widgets
                  )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create device super widget
     */
    public Optical(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class)
          );
    }
  };

  /** Device super widget
   */
  static class Device extends Composite
  {
    enum WidgetTypes
    {
      VOLUME_SIZE
    };

    Text   deviceName;
    Combo  volumeSize;

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     * @param deviceNameVariable device name variable (can be null)
     * @param volumeSizeVariable volume size variable (can be null)
     */
    public Device(final Composite      composite,
                  EnumSet<WidgetTypes> widgets,
                  final WidgetVariable deviceNameVariable,
                  final WidgetVariable volumeSizeVariable
                 )
    {
      super(composite,SWT.NONE);
      setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));

      int       row;
      Composite subComposite;
      Label     label;
      Spinner   spinner;
      Text      text;
      Button    button;
      Combo     combo;

      row = 0;

      label = Widgets.newLabel(this,BARControl.tr("Device")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(this,SWT.NONE);
      subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        deviceName = Widgets.newText(subComposite);
        deviceName.setToolTipText(BARControl.tr("Device name. Leave it empty to use system default device name."));
        Widgets.layout(deviceName,0,0,TableLayoutData.WE);
        deviceName.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            String string = widget.getText();

            if (deviceNameVariable != null)
            {
              Color color = COLOR_MODIFIED;
              if (deviceNameVariable.getString().equals(string)) color = null;
              widget.setBackground(color);
              widget.setData("showedErrorDialog",false);
            }
          }
        });
        deviceName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;

            if (deviceNameVariable != null)
            {
              deviceNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        deviceName.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            widget.setData("showedErrorDialog",false);
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            Text widget = (Text)focusEvent.widget;

            if (deviceNameVariable != null)
            {
              deviceNameVariable.set(widget.getText());
              widget.setBackground(null);
            }
          }
        });
        if (deviceNameVariable != null)
        {
          Widgets.addModifyListener(new WidgetModifyListener(deviceName,deviceNameVariable));
        }

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        button.setToolTipText(BARControl.tr("Select remote device. CTRL+click to select local device."));
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String fileName;

            fileName = Dialogs.file(composite.getShell(),
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select device name"),
                                    deviceName.getText(),
                                    new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                },
                                    "*",
                                    ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                      ? BARServer.remoteListDirectory
                                      : BARControl.listDirectory
                                   );
            if (fileName != null)
            {
              deviceName.setText(fileName);
              if (deviceNameVariable != null)
              {
                deviceNameVariable.set(fileName);
              }
            }
          }
        });
      }
      row++;

      if (widgets.contains(WidgetTypes.VOLUME_SIZE))
      {
        label = Widgets.newLabel(this,BARControl.tr("Size")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subComposite = Widgets.newComposite(this,SWT.NONE);
        Widgets.layout(subComposite,row,1,TableLayoutData.WE);
        {
          volumeSize = Widgets.newCombo(subComposite);
          volumeSize = BARWidgets.newByteSize(subComposite,
                                              BARControl.tr("Size of medium. You may specify a smaller value than the real physical size to leave some free space for error-correction codes."),
                                              volumeSizeVariable,
                                              new Object[]{Units.formatSize(       425*Units.M ),       425*Units.M,
                                                           Units.formatSize(       430*Units.M ),       430*Units.M,
                                                           Units.formatSize(       470*Units.M ),       470*Units.M,
                                                           Units.formatSize(       520*Units.M ),       520*Units.M,
                                                           Units.formatSize(       560*Units.M ),       560*Units.M,
                                                           Units.formatSize(       650*Units.M ),       650*Units.M,
                                                           Units.formatSize(       660*Units.M ),       660*Units.M,
                                                           Units.formatSize(       720*Units.M ),       720*Units.M,
                                                           Units.formatSize(       850*Units.M ),       850*Units.M,
                                                           Units.formatSize(         2*Units.G ),         2*Units.G,
                                                           Units.formatSize((long)(3.2*Units.G)),(long)(3.2*Units.G),
                                                           Units.formatSize(         4*Units.G ),         4*Units.G,
                                                           Units.formatSize((long)(6.4*Units.G)),(long)(6.4*Units.G),
                                                           Units.formatSize((long)(7.2*Units.G)),(long)(7.2*Units.G),
                                                           Units.formatSize(         8*Units.G ),         8*Units.G,
                                                           Units.formatSize(        20*Units.G ),        20*Units.G,
                                                           Units.formatSize(        25*Units.G ),        25*Units.G,
                                                           Units.formatSize(        40*Units.G ),        40*Units.G,
                                                           Units.formatSize(        50*Units.G ),        50*Units.G,
                                                           Units.formatSize(        80*Units.G ),        80*Units.G,
                                                           Units.formatSize(       100*Units.G ),       100*Units.G,
                                                           Units.formatSize(         1*Units.T ),         1*Units.T,
                                                           Units.formatSize(         2*Units.T ),         2*Units.T,
                                                           Units.formatSize(         4*Units.T ),         4*Units.T,
                                                           Units.formatSize(         5*Units.T ),         8*Units.T,
                                                           Units.formatSize(        16*Units.T ),        16*Units.T,
                                                           Units.formatSize(        32*Units.T ),        32*Units.T
                                                          }
                                             );
          volumeSize.setData("showedErrorDialog",false);
          Widgets.layout(volumeSize,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
        }
        row++;
      }

      setVisible(false);
    }

    /** create device super widget
     * @param composite parent composite
     * @param deviceNameVariable device name variable (can be null)
     * @param volumeSizeVariable volume size variable (can be null)
     */
    public Device(final Composite      composite,
                  final WidgetVariable deviceNameVariable,
                  final WidgetVariable volumeSizeVariable
                 )
    {
      this(composite,
           EnumSet.allOf(WidgetTypes.class),
           deviceNameVariable,
           volumeSizeVariable
          );
    }

    /** create device super widget
     * @param composite parent composite
     * @param widgets optional widgets to show
     */
    public Device(final Composite      composite,
                  EnumSet<WidgetTypes> widgets
                 )
    {
      this(composite,
           widgets,
           (WidgetVariable)null,
           (WidgetVariable)null
          );
    }

    /** create device super widget
     * @param composite parent composite
     */
    public Device(Composite composite)
    {
      this(composite,
           EnumSet.noneOf(WidgetTypes.class)
          );
    }
  };
}

/* end of file */
