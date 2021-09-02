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
import org.eclipse.swt.widgets.Display;
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @return text widget
   */
  public static Text newText(Composite            parentComposite,
                             String               toolTipText,
                             final WidgetVariable widgetVariable,
                             final Listener       listener
                            )
  {
    final Text text;

    text = Widgets.newText(parentComposite,SWT.LEFT|SWT.BORDER);
    text.setToolTipText(toolTipText);
    text.setText(widgetVariable.getString());
    text.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String s = widget.getText();
        if (widgetVariable.getString().equals(s)) color = null;

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
          public void modified(Widget widget, WidgetVariable variable)
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return text widget
   */
  public static Text newText(Composite      parentComposite,
                             String         toolTipText,
                             WidgetVariable widgetVariable
                            )
  {
    return newText(parentComposite,toolTipText,widgetVariable,(Listener)null);
  }

  /** create new number widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param min,max min/max value
   * @param listener listener or null
   * @return number widget
   */
  public static Spinner newNumber(Composite            parentComposite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  int                  min,
                                  int                  max,
                                  final Listener       listener
                                 )
  {
    final Spinner spinner;

    spinner = Widgets.newSpinner(parentComposite);
    spinner.setToolTipText(toolTipText);
    spinner.setMinimum(min);
    spinner.setMaximum(max);
    spinner.setSelection(widgetVariable.getInteger());

    spinner.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Spinner widget = (Spinner)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        int n = widget.getSelection();

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
          public void modified(Widget widget, WidgetVariable variable)
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param min,max min/max value
   * @return number widget
   */
  public static Spinner newNumber(Composite      parentComposite,
                                  String         toolTipText,
                                  WidgetVariable widgetVariable,
                                  int            min,
                                  int            max
                                 )
  {
    return newNumber(parentComposite,toolTipText,widgetVariable,min,max,(Listener)null);
  }

  /** create new checkbox widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param text checkbox text
   * @param listener listener or null
   * @return number widget
   */
  public static Button newCheckbox(Composite            parentComposite,
                                   String               toolTipText,
                                   final WidgetVariable widgetVariable,
                                   String               text,
                                   final Listener       listener
                                  )
  {
    final Button button;

    button = Widgets.newCheckbox(parentComposite,text);
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
          public void modified(Widget widget, WidgetVariable variable)
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param text checkbox text
   * @return number widget
   */
  public static Button newCheckbox(Composite      parentComposite,
                                   String         toolTipText,
                                   WidgetVariable widgetVariable,
                                   String         text
                                  )
  {
    return newCheckbox(parentComposite,toolTipText,widgetVariable,text,(Listener)null);
  }

  /** create new byte size widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values
   * @param listener listener or null
   * @return number widget
   */
  public static Combo newByteSize(Composite            parentComposite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  String[]             values,
                                  final Listener       listener
                                 )
  {
    final Shell shell = parentComposite.getShell();
    final Combo combo;

    combo = Widgets.newCombo(parentComposite);
    combo.setToolTipText(toolTipText);
    combo.setItems(values);
    combo.setText(widgetVariable.getString());
    combo.setData("showedErrorDialog",false);

    combo.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Combo widget = (Combo)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String s = widget.getText();
        if (listener != null)
        {
          if (listener.getString(widgetVariable).equals(s)) color = null;
        }
        else
        {
          if (widgetVariable.getString().equals(s)) color = null;
        }
/*
        try
        {
          long n = Units.parseByteSize(widget.getText());
          if (widgetVariable.getLong() == n) color = null;
        }
        catch (NumberFormatException exception)
        {
        }*/
        widget.setBackground(color);
        widget.setData("showedErrorDialog",false);
      }
    });
    combo.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            long n = Units.parseByteSize(string);
            string = Units.formatByteSize(n);
          }
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

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }
        widget.setText(string);
        widget.setBackground(null);
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Combo  widget = (Combo)selectionEvent.widget;
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            long  n = Units.parseByteSize(string);
            string = Units.formatByteSize(n);
          }
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

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setText(string);
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
        String string = widget.getText();
        try
        {
          if (!string.isEmpty())
          {
            long n = Units.parseByteSize(string);
            string = Units.formatByteSize(n);
          }
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

        if (listener != null)
        {
          listener.setString(widgetVariable,string);
        }
        else
        {
          widgetVariable.set(string);
        }

        widget.setText(string);
        widget.setBackground(null);
      }
    });

    final WidgetModifyListener widgetModifiedListener = (listener != null)
      ? new WidgetModifyListener(combo,widgetVariable)
        {
          public void modified(Widget widget, WidgetVariable variable)
          {
            combo.setText(listener.getString(widgetVariable));
          }
        }
      : new WidgetModifyListener(combo,widgetVariable)
        {
          public String getString(WidgetVariable variable)
          {
            return variable.getString();
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

    if (listener != null)
    {
      combo.setText(listener.getString(widgetVariable));
    }
    else
    {
      combo.setText(widgetVariable.getString());
    }

    return combo;
  }

  /** create new byte size widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values
   * @return number widget
   */
  public static Combo newByteSize(Composite      parentComposite,
                                  String         toolTipText,
                                  WidgetVariable widgetVariable,
                                  String[]       values
                                 )
  {
    return newByteSize(parentComposite,toolTipText,widgetVariable,values,(Listener)null);
  }

  /** create new time widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values [text,value]
   * @param listener listener or null
   * @return number widget
   */
  public static Combo newTime(Composite            parentComposite,
                              String               toolTipText,
                              final WidgetVariable widgetVariable,
                              String[]             values,
                              final Listener       listener
                             )
  {
    final Shell shell = parentComposite.getShell();
    final Combo combo;

    combo = Widgets.newCombo(parentComposite);
    combo.setToolTipText(toolTipText);
    combo.setItems(values);
    combo.setData("showedErrorDialog",false);

    combo.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Combo widget = (Combo)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String string = widget.getText();
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
          public void modified(Widget widget, WidgetVariable variable)
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param values combo values
   * @return number widget
   */
  public static Combo newTime(Composite      parentComposite,
                              String         toolTipText,
                              WidgetVariable widgetVariable,
                              String[]       values
                             )
  {
    return newTime(parentComposite,toolTipText,widgetVariable,values,(Listener)null);
  }

  /** create new path name widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return text widget
   */
  public static Composite newDirectory(Composite            parentComposite,
                                       String               toolTipText,
                                       final WidgetVariable widgetVariable
                                      )
  {
    final Shell shell = parentComposite.getShell();
    Composite   composite;
    final Text  text;
    Button      button;

    composite = Widgets.newComposite(parentComposite,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      text = Widgets.newText(composite,SWT.LEFT|SWT.BORDER);
      text.setToolTipText(toolTipText);
      text.setText(widgetVariable.getString());
      Widgets.layout(text,0,0,TableLayoutData.WE);
      text.addModifyListener(new ModifyListener()
      {
        public void modifyText(ModifyEvent modifyEvent)
        {
          Text  widget = (Text)modifyEvent.widget;
          Color color  = COLOR_MODIFIED;

          String s = widget.getText();
          if (widgetVariable.getString().equals(s)) color = null;

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

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
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

    return composite;
  }

  /** create new file name widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return text widget
   */
  public static Composite newFile(Composite            parentComposite,
                                  String               toolTipText,
                                  final WidgetVariable widgetVariable,
                                  final String[]       fileExtensions,
                                  final String         defaultFileExtension
                                 )
  {
    final Shell shell = parentComposite.getShell();
    Composite   composite;
    final Text  text;
    Button      button;

    composite = Widgets.newComposite(parentComposite,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      text = Widgets.newText(composite,SWT.LEFT|SWT.BORDER);
      text.setToolTipText(toolTipText);
      text.setText(widgetVariable.getString());
      Widgets.layout(text,0,0,TableLayoutData.WE);
      text.addModifyListener(new ModifyListener()
      {
        public void modifyText(ModifyEvent modifyEvent)
        {
          Text  widget = (Text)modifyEvent.widget;
          Color color  = COLOR_MODIFIED;

          String s = widget.getText();
          if (widgetVariable.getString().equals(s)) color = null;

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

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
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

    return composite;
  }

  /** create new password widget
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @param listener listener or null
   * @return text widget
   */
  public static Text newPassword(Composite            parentComposite,
                                 String               toolTipText,
                                 final WidgetVariable widgetVariable,
                                 final Listener       listener
                                )
  {
    final Text text;

    text = Widgets.newText(parentComposite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
    text.setToolTipText(toolTipText);
    text.setText(widgetVariable.getString());
    text.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        Color color  = COLOR_MODIFIED;

        String s = widget.getText();
        if (widgetVariable.getString().equals(s)) color = null;

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
          public void modified(Widget widget, WidgetVariable variable)
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
   * @param parentComposite parent composite
   * @param toolTipText tooltip text
   * @param widgetVariable widget variable
   * @return text widget
   */
  public static Text newPassword(Composite      parentComposite,
                                 String         toolTipText,
                                 WidgetVariable widgetVariable
                                )
  {
    return newPassword(parentComposite,toolTipText,widgetVariable,(Listener)null);
  }
}

/* end of file */
