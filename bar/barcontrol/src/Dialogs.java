/***********************************************************************\
*
* $Source: /tmp/cvs/onzen/src/Dialogs.java,v $
* $Revision: 949 $
* $Author: trupp $
* Contents: dialog functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.reflect.Field;
import java.io.File;
import java.io.IOException;
import java.util.BitSet;
import java.util.List;

import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Monitor;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

/** simply busy dialog
 */
class SimpleBusyDialog
{
  /** dialog data
   */
  class Data
  {
    int     animationIndex;
    boolean animationQuit;

    Data()
    {
      this.animationIndex = 0;
      this.animationQuit  = false;
    }
  };

  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private final Data    data = new Data();
  private final Display display;
  private final Shell   dialog;
  private final Image   image;
  private final Point   imageSize;
  private final Canvas  widgetImage;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param image image to show
   * @param imageSize size of image
   * @param message message to show
   * @param abortButton true for abort-button, false otherwise
   */
  SimpleBusyDialog(Shell parentShell, String title, Image image, Point imageSize, String message, boolean abortButton)
  {
    Composite composite;
    Label     label;
    Button    button;

    this.image     = image;
    this.imageSize = imageSize;

    display = parentShell.getDisplay();

    dialog = Dialogs.openModal(parentShell,title,250,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    // message
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      widgetImage = new Canvas(composite,SWT.LEFT);
      widgetImage.setSize(imageSize.x,imageSize.y);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.N|TableLayoutData.WE,0,0,4));
    }

    if (abortButton)
    {
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,1.0,4));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));
      {
        button = new Button(composite,SWT.CENTER);
        button.setText("Abort");
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close();
          }
        });
      }
    }

    Dialogs.show(dialog);
    animate();
  }

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param image image to show
   * @param imageSize size of image
   * @param message message to show
   */
  SimpleBusyDialog(Shell parentShell, String title, Image image, Point imageSize, String message)
  {
    this(parentShell,title,image,imageSize,message,false);
  }

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param image image to show
   * @param message message to show
   * @param abortButton true for abort-button, false otherwise
   */
  SimpleBusyDialog(Shell parentShell, String title, Image image, String message, boolean abortButton)
  {
    this(parentShell,title,image,new Point(48,48),message,abortButton);
  }

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param image image to show
   * @param message message to show
   */
  SimpleBusyDialog(Shell parentShell, String title, Image image, String message)
  {
    this(parentShell,title,image,message,false);
  }

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param message message to show
   * @param abortButton true for abort-button, false otherwise
   */
  SimpleBusyDialog(Shell parentShell, String title, String message, boolean abortButton)
  {
    this(parentShell,title,Widgets.loadImage(parentShell.getDisplay(),"working.png"),message,abortButton);
  }

  /** create simple busy dialog
   * @param parentShell parent shell
   * @param title window title
   * @param message message to show
   */
  SimpleBusyDialog(Shell parentShell, String title, String message)
  {
    this(parentShell,title,message,false);
  }

  /** animate dialog
   */
  public void animate()
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          int x = (data.animationIndex%8)*48;
          int y = (data.animationIndex/8)*48;

          if (!widgetImage.isDisposed())
          {
            GC gc = new GC(widgetImage);
            widgetImage.drawBackground(gc,0,0,imageSize.x,imageSize.y);
            gc.drawImage(image,
                         x,y,imageSize.x,imageSize.y,
                         0,0,imageSize.x,imageSize.y
                        );
            gc.dispose();
            display.update();
          }

          data.animationIndex = 1+(data.animationIndex % 23);
        }
      });
    }
  }

  /** auto animate dialog
   * @param timeInterval animate time interval [ms]
   */
  public void autoAnimate(final int timeInterval)
  {
    Thread thread = new Thread()
    {
      public void run()
      {
        while (!data.animationQuit)
        {
          animate();
          try { Thread.sleep(timeInterval); } catch (InterruptedException exception) { /* ignore */ }
        }
      }
    };
    thread.start();
  }

  /** auto animate dialog
   */
  public void autoAnimate()
  {
    autoAnimate(500);
  }

  /** close busy dialog
   */
  public void close()
  {
    data.animationQuit = true;
    if (!dialog.isDisposed())
    {
      Dialogs.close(dialog);
    }
  }

  /** check if dialog is closed
   * @return true iff closed
   */
  public boolean isClosed()
  {
    return dialog.isDisposed();
  }
}

// ------------------------------------------------------------------------

/** simply progress dialog
 */
class SimpleProgressDialog
{
  /** dialog data
   */
  class Data
  {
    int     animationIndex;
    boolean animationQuit;

    Data()
    {
      this.animationIndex = 0;
      this.animationQuit  = false;
    }
  };

  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private final Data        data = new Data();
  private final Display     display;
  private final Shell       dialog;
  private final ProgressBar widgetProgressBar;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create simgple progress dialog
   * @param parentShell parent shell
   * @param title window title
   * @param message message to show
   */
  SimpleProgressDialog(Shell parentShell, String title, String message)
  {
    Composite composite;
    Label     label;
    Button    button;

    display = parentShell.getDisplay();

    dialog = Dialogs.openModal(parentShell,title,250,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    // message
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NS|TableLayoutData.W,0,0,4));

      widgetProgressBar = new ProgressBar(composite);
      widgetProgressBar.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NS|TableLayoutData.W,0,0,4));
    }

    Dialogs.show(dialog);
    animate();
  }

  /** animate dialog
   */
  public void animate()
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          int x = (data.animationIndex%8)*48;
          int y = (data.animationIndex/8)*48;
Dprintf.dprintf("");

/*
          if (!widgetImage.isDisposed())
          {
            GC gc = new GC(widgetImage);
            widgetImage.drawBackground(gc,0,0,imageSize.x,imageSize.y);
            gc.drawImage(image,
                         x,y,imageSize.x,imageSize.y,
                         0,0,imageSize.x,imageSize.y
                        );
            gc.dispose();
            display.update();
          }
*/

          data.animationIndex = (data.animationIndex+1) % 24;
        }
      });
    }
  }

  /** auto animate dialog
   * @param timeInterval animate time interval [ms]
   */
  public void autoAnimate(final int timeInterval)
  {
    Thread thread = new Thread()
    {
      public void run()
      {
        while (!data.animationQuit)
        {
          animate();
          try { Thread.sleep(timeInterval); } catch (InterruptedException exception) { /* ignore */ }
        }
      }
    };
    thread.start();
  }

  /** auto animate dialog
   */
  public void autoAnimate()
  {
    autoAnimate(500);
  }

  /** close progress dialog
   */
  public void close()
  {
    data.animationQuit = true;
    Dialogs.close(dialog);
  }
}

// ------------------------------------------------------------------------

/** dialog runnable
 */
abstract class DialogRunnable
{
  /** executed when dialog is done
   * @param result result
   */
  abstract public void done(Object result);
}

/** boolean field updater
 */
class BooleanFieldUpdater
{
  private Object object;
  private Field  field;

  /** create boolean field updater
   * @param clazz class with boolean or Boolean field
   * @param fieldName field name
   */
  BooleanFieldUpdater(Class clazz, String fieldName)
  {
    try
    {
      this.object = object;
      this.field  = clazz.getDeclaredField(fieldName);
    }
    catch (NoSuchFieldException exception)
    {
      throw new Error(exception);
    }
    catch (SecurityException exception)
    {
      throw new Error(exception);
    }
  }

  /** get boolean field value
   * @return value
   */
  public boolean get()
  {
    try
    {
      if (field.getType() == Boolean.class)
      {
        return (Boolean)field.get(object);
      }
      else
      {
        return field.getBoolean(object);
      }
    }
    catch (IllegalAccessException exception)
    {
      throw new Error(exception);
    }
  }

  /** set boolean field value
   * @param value value
   */
  public void set(boolean value)
  {
    try
    {
      field.set(object,new Boolean(value));
    }
    catch (IllegalAccessException exception)
    {
      throw new Error(exception);
    }
  }
}

/** dialog
 */
class Dialogs
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  private static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double columnWeight, int style)
  {
    TableLayout tableLayout;

    // create dialog
    final Shell dialog;
    if ((style & SWT.APPLICATION_MODAL) == SWT.APPLICATION_MODAL)
    {
      dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    else
    {
      dialog = new Shell(parentShell.getDisplay(),SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    dialog.setText(title);
    tableLayout = new TableLayout(rowWeight,columnWeight,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    dialog.setLayout(tableLayout);

    return dialog;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double columnWeight)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeight,columnWeight,SWT.NONE);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double columnWeight)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeight,columnWeight,SWT.APPLICATION_MODAL);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, double rowWeight, double columnWeight)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeight,columnWeight);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, double rowWeight, double columnWeight)
  {
    return openModal(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeight,columnWeight);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  private static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double columnWeight, int style)
  {
    TableLayout tableLayout;

    // create dialog
    final Shell dialog;
    if ((style & SWT.APPLICATION_MODAL) == SWT.APPLICATION_MODAL)
    {
      dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    else
    {
      dialog = new Shell(parentShell.getDisplay(),SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    dialog.setText(title);
    tableLayout = new TableLayout(rowWeights,columnWeight,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    dialog.setLayout(tableLayout);

    return dialog;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double columnWeight)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeights,columnWeight,SWT.RESIZE);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double columnWeight)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeights,columnWeight,SWT.RESIZE|SWT.APPLICATION_MODAL);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, double[] rowWeights, double columnWeight)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeights,columnWeight);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, double[] rowWeights, double columnWeight)
  {
    return openModal(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeights,columnWeight);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  private static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double[] columnWeights, int style)
  {
    TableLayout tableLayout;

    // create dialog
    final Shell dialog;
    if ((style & SWT.APPLICATION_MODAL) == SWT.APPLICATION_MODAL)
    {
      dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    else
    {
      dialog = new Shell(parentShell.getDisplay(),SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    dialog.setText(title);
    tableLayout = new TableLayout(rowWeight,columnWeights,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    dialog.setLayout(tableLayout);

    return dialog;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double[] columnWeights)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeight,columnWeights,SWT.NONE);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double[] columnWeights)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeight,columnWeights,SWT.APPLICATION_MODAL);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, double rowWeight, double[] columnWeights)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeight,columnWeights);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, double rowWeight, double[] columnWeights)
  {
    return openModal(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeight,columnWeights);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  private static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights, int style)
  {
    TableLayout tableLayout;

    // create dialog
    final Shell dialog;
    if ((style & SWT.APPLICATION_MODAL) == SWT.APPLICATION_MODAL)
    {
      dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    else
    {
      dialog = new Shell(parentShell.getDisplay(),SWT.DIALOG_TRIM|SWT.RESIZE|style);
    }
    dialog.setText(title);
    tableLayout = new TableLayout(rowWeights,columnWeights,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    dialog.setLayout(tableLayout);

    return dialog;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeights,columnWeights,SWT.NONE);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights)
  {
    return open(parentShell,title,minWidth,minHeight,rowWeights,columnWeights,SWT.APPLICATION_MODAL);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, double[] rowWeights, double[] columnWeights)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeights,columnWeights);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, double[] rowWeights, double[] columnWeights)
  {
    return openModal(parentShell,title,SWT.DEFAULT,SWT.DEFAULT,rowWeights,columnWeights);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title, int minWidth, int minHeight)
  {
    return open(parentShell,title,minWidth,minHeight,new double[]{1,0},1.0);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title, int minWidth, int minHeight)
  {
    return openModal(parentShell,title,minWidth,minHeight,new double[]{1,0},1.0);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @return dialog shell
   */
  public static Shell open(Shell parentShell, String title)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** open a new modal dialog
   * @param parentShell parent shell
   * @param title title string
   * @return dialog shell
   */
  public static Shell openModal(Shell parentShell, String title)
  {
    return openModal(parentShell,title,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  public static void close(Shell dialog, Object returnValue)
  {
    dialog.setData(returnValue);
    dialog.close();
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  public static void close(Shell dialog)
  {
    close(dialog,null);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param location top/left location or null
   * @param size size of dialog or null
   * @param setLocationFlag TRUE iff location of dialog should be set
   */
  public static void show(Shell dialog, Point location, Point size, boolean setLocationFlag)
  {
    int x,y;

    if (!dialog.isVisible())
    {
      Display display = dialog.getDisplay();

      // layout
      dialog.pack();

      if (setLocationFlag)
      {
        // set location of dialog
        Point cursorPoint = display.getCursorLocation();
        Rectangle dialogBounds = dialog.getBounds();
        for (Monitor monitor : display.getMonitors())
        {
          if (monitor.getBounds().contains(cursorPoint))
          {
            Rectangle monitorBounds = monitor.getClientArea();
            x = ((location != null) && (location.x != SWT.DEFAULT))
              ? location.x
              : Math.max(monitorBounds.x,
                         Math.min(monitorBounds.x+monitorBounds.width-dialogBounds.width,
                                  cursorPoint.x-dialogBounds.width/2
                                 )
                        );
            y = ((location != null) && (location.y != SWT.DEFAULT))
              ? location.y
              : Math.max(monitorBounds.y,
                         Math.min(monitorBounds.y+monitorBounds.height-dialogBounds.height,
                                  cursorPoint.y-dialogBounds.height/2
                                 )
                        );
            dialog.setLocation(x,y);
            break;
          }
        }
      }

      // set size (if given)
      if (size != null)
      {
        Point newSize = dialog.getSize();
        if (size.x != SWT.DEFAULT) newSize.x = size.x;
        if (size.y != SWT.DEFAULT) newSize.y = size.y;
        dialog.setSize(newSize);
      }

      // open dialog
      dialog.open();

      // update all
      display.update();
    }
  }

    /** show dialog
   * @param dialog dialog shell
   * @param location top/left location or null
   * @param size size of dialog or null
   */
  public static void show(Shell dialog, Point location, Point size)
  {
    show(dialog,location,size,true);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param size size of dialog or null
   * @param setLocationFlag TRUE iff location of dialog should be set
   */
  public static void show(Shell dialog, Point size, boolean setLocationFlag)
  {
    show(dialog,null,size,setLocationFlag);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param size size of dialog or null
   */
  public static void show(Shell dialog, Point size)
  {
    show(dialog,null,size);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param width,height width/height of dialog
   * @param setLocationFlag TRUE iff location of dialog should be set
   */
  public static void show(Shell dialog, int width, int height, boolean setLocationFlag)
  {
    show(dialog,new Point(width,height),setLocationFlag);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param width,height width/height of dialog
   */
  public static void show(Shell dialog, int width, int height)
  {
    show(dialog,new Point(width,height),true);
  }

  /** show dialog
   * @param dialog dialog shell
   * @param setLocationFlag TRUE iff location of dialog should be set
   */
  public static void show(Shell dialog, boolean setLocationFlag)
  {
    show(dialog,null,setLocationFlag);
  }

  /** show dialog
   * @param dialog dialog shell
   */
  public static void show(Shell dialog)
  {
    show(dialog,true);
  }

  /** run dialog
   * @param dialog dialog shell
   * @param escapeKeyReturnValue value to return on ESC key
   */
  public static Object run(final Shell dialog, final Object escapeKeyReturnValue, final DialogRunnable dialogRunnable)
  {
    final Object[] result = new Object[1];

    if (!dialog.isDisposed())
    {
      Display display = dialog.getDisplay();

      // add escape key handler
      dialog.addTraverseListener(new TraverseListener()
      {
        public void keyTraversed(TraverseEvent traverseEvent)
        {
          Shell widget = (Shell)traverseEvent.widget;

          if (traverseEvent.detail == SWT.TRAVERSE_ESCAPE)
          {
            // store ESC result
            widget.setData(escapeKeyReturnValue);

            /* stop processing key, send close event. Note: this is required
               in case a widget in the dialog has a key-handler. Then the
               ESC key will not trigger an SWT.Close event.
            */
            traverseEvent.doit = false;
            Event event = new Event();
            event.widget = dialog;
            dialog.notifyListeners(SWT.Close,event);
          }
        }
      });

      // add close handler to get result
      dialog.addListener(SWT.Close,new Listener()
      {
        public void handleEvent(Event event)
        {
          // get result
          result[0] = dialog.getData();

          // set escape result if no result set
          if (result[0] == null) result[0] = escapeKeyReturnValue;

          // execute dialog runnable
          if (dialogRunnable != null)
          {
            dialogRunnable.done(result[0]);
          }

          // close the dialog
          dialog.dispose();
        }
      });

      // show
      show(dialog);

      if ((dialog.getStyle() & SWT.APPLICATION_MODAL) == SWT.APPLICATION_MODAL)
      {
        // run dialog
        while (!dialog.isDisposed())
        {
          if (!display.readAndDispatch()) display.sleep();
        }

        // update all
        display.update();
      }
      else
      {
        result[0] = null;
      }
    }

    return result[0];
  }

  /** run dialog
   * @param dialog dialog shell
   * @param escapeKeyReturnValue value to return on ESC key
   */
  public static Object run(final Shell dialog, final Object escapeKeyReturnValue)
  {
    return run(dialog,escapeKeyReturnValue,null);
  }

  /** run dialog
   * @param dialog dialog shell
   * @param closeRunnable
   */
  public static Object run(Shell dialog, DialogRunnable dialogRunnable)
  {
    return run(dialog,null,dialogRunnable);
  }

  /** run dialog
   * @param dialog dialog shell
   */
  public static Object run(Shell dialog)
  {
    return run(dialog,null);
  }

  /** create boolean field updater
   * @param clazz class with boolean or Boolean field
   * @param fieldName field name
   * @return boolean field updater
   */
  public static BooleanFieldUpdater booleanFieldUpdater(Class clazz, String fieldName)
  {
    return new BooleanFieldUpdater(clazz,fieldName);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param title title text
   * @param image image to show
   * @param message info message
   */
  public static void info(Shell parentShell, String title, final BooleanFieldUpdater showAgainFieldFlag, Image image, String message)
  {
    Composite composite;
    Label     label;
    Button    button;

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final Shell dialog = openModal(parentShell,title,300,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          label = new Label(composite,SWT.LEFT);
          label.setImage(image);
          label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NS|TableLayoutData.W,0,0,4));

          if (showAgainFieldFlag != null)
          {
            widgetShowAgain = new Button(composite,SWT.CHECK);
            widgetShowAgain.setText("show again");
            widgetShowAgain.setSelection(true);
            widgetShowAgain.setLayoutData(new TableLayoutData(1,1,TableLayoutData.W));
          }
          else
          {
            widgetShowAgain = null;
          }
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
        {
          button = new Button(composite,SWT.CENTER);
          button.setText("Close");
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        run(dialog,new DialogRunnable()
        {
          public void done(Object result)
          {
            if (showAgainFieldFlag != null)
            {
              showAgainFieldFlag.set(widgetShowAgain.getSelection());
            }
          }
        });
      }
    }
  }

  /** info dialog
   * @param parentShell parent shell
   * @param title title text
   * @param image image to show
   * @param message info message
   */
  public static void info(Shell parentShell, String title, Image image, String message)
  {
    info(parentShell,title,(BooleanFieldUpdater)null,image,message);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param title title text
   * @param message info message
   */
  public static void info(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"info.png");

    info(parentShell,title,showAgainFieldFlag,IMAGE,message);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param title title text
   * @param message info message
   */
  public static void info(Shell parentShell, String title, String message)
  {
    info(parentShell,title,(BooleanFieldUpdater)null,message);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  public static void info(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    info(parentShell,"Information",showAgainFieldFlag,message);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  public static void info(Shell parentShell, String message)
  {
    info(parentShell,(BooleanFieldUpdater)null,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message error message
   */
  public static void error(Shell parentShell, final BooleanFieldUpdater showAgainFieldFlag, String[] extendedMessage, String message)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"error.png");

    Composite composite;
    Label     label;
    Button    button;
    Text      text;

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final Shell dialog = openModal(parentShell,"Error",300,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          label = new Label(composite,SWT.LEFT);
          label.setImage(IMAGE);
          label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0,4));

          int row = 1;

          if (extendedMessage != null)
          {
            label = new Label(composite,SWT.LEFT);
            label.setText("Extended error:");
            label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4)); row++;

            text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.READ_ONLY);
            text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,100)); row++;
            text.setText(StringUtils.join(extendedMessage,text.DELIMITER));
          }

          if (showAgainFieldFlag != null)
          {
            widgetShowAgain = new Button(composite,SWT.CHECK);
            widgetShowAgain.setText("show again");
            widgetShowAgain.setSelection(true);
            widgetShowAgain.setLayoutData(new TableLayoutData(1,1,TableLayoutData.W));
          }
          else
          {
            widgetShowAgain = null;
          }
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
        {
          button = new Button(composite,SWT.CENTER);
          button.setText("Close");
          button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        run(dialog,new DialogRunnable()
        {
          public void done(Object result)
          {
            if (showAgainFieldFlag != null)
            {
              showAgainFieldFlag.set(widgetShowAgain.getSelection());
            }
          }
        });
      }
    }
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message error message
   */
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, List<String> extendedMessage, String message)
  {
    error(parentShell,showAgainFieldFlag,extendedMessage.toArray(new String[extendedMessage.size()]),message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message error message
   */
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    error(parentShell,showAgainFieldFlag,(String[])null,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   */
  public static void error(Shell parentShell, String message)
  {
    error(parentShell,(BooleanFieldUpdater)null,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String[] extendedMessage, String format, Object... arguments)
  {
    error(parentShell,showAgainFieldFlag,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, List<String> extendedMessage, String format, Object... arguments)
  {
    error(parentShell,showAgainFieldFlag,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, String[] extendedMessage, String format, Object... arguments)
  {
    error(parentShell,(BooleanFieldUpdater)null,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, List<String> extendedMessage, String format, Object... arguments)
  {
    error(parentShell,(BooleanFieldUpdater)null,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String format, Object... arguments)
  {
    error(parentShell,showAgainFieldFlag,(String[])null,format,arguments);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell parentShell, String format, Object... arguments)
  {
    error(parentShell,(BooleanFieldUpdater)null,format,arguments);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message error message
   */
  static void warning(Shell parentShell, final BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"warning.png");

    final boolean[] result = new boolean[]{true};
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final Shell dialog = open(parentShell,"Warning",200,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          label = new Label(composite,SWT.LEFT);
          label.setImage(IMAGE);
          label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0,4));

          if (showAgainFieldFlag != null)
          {
            widgetShowAgain = new Button(composite,SWT.CHECK);
            widgetShowAgain.setText("show again");
            widgetShowAgain.setSelection(true);
            widgetShowAgain.setLayoutData(new TableLayoutData(1,1,TableLayoutData.W));
          }
          else
          {
            widgetShowAgain = null;
          }
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
        {
          button = new Button(composite,SWT.CENTER);
          button.setText("Close");
          button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,60,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              close(dialog);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        run(dialog,new DialogRunnable()
        {
          public void done(Object result)
          {
            if (showAgainFieldFlag != null)
            {
              showAgainFieldFlag.set(widgetShowAgain.getSelection());
            }
          }
        });
      }
    }
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void warning(Shell parentShell, String message)
  {
    warning(parentShell,(BooleanFieldUpdater)null,message);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String format, Object... arguments)
  {
    warning(parentShell,showAgainFieldFlag,String.format(format,arguments));
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell parentShell, String format, Object... arguments)
  {
    warning(parentShell,(BooleanFieldUpdater)null,format,arguments);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value if no show-again field updater or show-again checkbox is true,
             defaultValue otherwise
   */
  public static boolean confirm(Shell parentShell, String title, final BooleanFieldUpdater showAgainFieldFlag, Image image, String message, String yesText, String noText, boolean defaultValue)
  {
    Composite composite;
    Label     label;
    Button    button;

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final boolean[] result = new boolean[1];

        final Shell dialog = openModal(parentShell,title,300,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          label = new Label(composite,SWT.LEFT);
          label.setImage(image);
          label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0,4));

          if (showAgainFieldFlag != null)
          {
            widgetShowAgain = new Button(composite,SWT.CHECK);
            widgetShowAgain.setText("show again");
            widgetShowAgain.setSelection(true);
            widgetShowAgain.setLayoutData(new TableLayoutData(1,1,TableLayoutData.W));
          }
          else
          {
            widgetShowAgain = null;
          }
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
        {
          button = new Button(composite,SWT.CENTER);
          button.setText(yesText);
          if (defaultValue == true) button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog,true);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = new Button(composite,SWT.CENTER);
          button.setText(noText);
          if (defaultValue == false) button.setFocus();
          button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog,false);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        return (Boolean)run(dialog,
                            new DialogRunnable()
                            {
                              public void done(Object result)
                              {
                                if (showAgainFieldFlag != null)
                                {
                                  showAgainFieldFlag.set(widgetShowAgain.getSelection());
                                }
                              }
                            }
                           );
      }
      else
      {
        return false;
      }
    }
    else
    {
      return defaultValue;
    }
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, Image image, String message, String yesText, String noText, boolean defaultValue)
  {
    return confirm(parentShell,title,null,image,message,yesText,noText,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, Image image, String message, String yesText, String noText)
  {
    return confirm(parentShell,title,showAgainFieldFlag,image,message,yesText,noText,true);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, Image image, String message, String yesText, String noText)
  {
    return confirm(parentShell,title,null,image,message,yesText,noText);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message, String yesText, String noText, boolean defaultValue)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return confirm(parentShell,title,showAgainFieldFlag,IMAGE,message,yesText,noText,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, String message, String yesText, String noText, boolean defaultValue)
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message,yesText,noText,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message, String yesText, String noText)
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,yesText,noText,true);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, String message, String yesText, String noText)
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message,yesText,noText);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message, boolean defaultValue)
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,"Yes","No",defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, String message, boolean defaultValue)
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,"Yes","No",false);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, String message)
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String message, boolean defaultValue)
  {
    return confirm(parentShell,"Confirmation",showAgainFieldFlag,message,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String message, boolean defaultValue)
  {
    return confirm(parentShell,(BooleanFieldUpdater)null,message,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, String message)
  {
    return confirm(parentShell,showAgainFieldFlag,message,false);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell parentShell, String message)
  {
    return confirm(parentShell,(BooleanFieldUpdater)null,message);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell parentShell, String title, BooleanFieldUpdater showAgainFieldFlag, String message, String yesText, String noText)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"error.png");

    return confirm(parentShell,title,IMAGE,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell parentShell, String title, String message, String yesText, String noText)
  {
    return confirmError(parentShell,title,(BooleanFieldUpdater)null,message,yesText,noText);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param okText ok-text
   * @param cancelText cancel-text
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1) or -1
   */
  public static int select(Shell parentShell, String title, String message, String[] texts, String[] helpTexts, boolean[] enabled, String okText, String cancelText, int defaultValue)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    Composite composite,subComposite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final int[] result = new int[1];

      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        // image
        label = new Label(composite,SWT.LEFT);
        label.setImage(IMAGE);
        label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

        subComposite = new Composite(composite,SWT.NONE);
        subComposite.setLayout(new TableLayout(0.0,1.0));
        subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE,0,0,4));
        {
          int row = 0;

          if (message != null)
          {
            // message
            label = new Label(subComposite,SWT.LEFT|SWT.WRAP);
            label.setText(message);
            label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE,0,0,4)); row++;
          }

          // buttons
          if ((okText != null) || (cancelText != null))
          {
            Button selectedButton = null;
            for (int z = 0; z < texts.length; z++)
            {
              button = new Button(subComposite,SWT.RADIO);
              button.setEnabled((enabled == null) || enabled[z]);
              button.setText(texts[z]);
              button.setData(z);
              if (   ((enabled == null) || enabled[z])
                  && ((z == defaultValue) || (selectedButton == null))
                 )
              {
                if (selectedButton != null) selectedButton.setSelection(false);

                button.setSelection(true);
                result[0] = z;
                selectedButton = button;
              }
              button.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W)); row++;
              button.addSelectionListener(new SelectionListener()
              {
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  Button widget = (Button)selectionEvent.widget;

                  result[0] = (Integer)widget.getData();
                }
              });
              if ((helpTexts != null) && (z < helpTexts.length))
              {
                button.setToolTipText(helpTexts[z]);
              }
            }
          }
        }
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        if ((okText != null) || (cancelText != null))
        {
          if (okText != null)
          {
            button = new Button(composite,SWT.CENTER);
            button.setText(okText);
            button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                close(dialog,true);
              }
            });
          }

          if (cancelText != null)
          {
            button = new Button(composite,SWT.CENTER);
            button.setText(cancelText);
            button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                close(dialog,false);
              }
            });
          }
        }
        else
        {
          int textWidth = 0;
          GC gc = new GC(composite);
          for (String text : texts)
          {
            textWidth = Math.max(textWidth,gc.textExtent(text).x);
          }
          gc.dispose();

          for (int z = 0; z < texts.length; z++)
          {
            button = new Button(composite,SWT.CENTER);
            button.setEnabled((enabled == null) || enabled[z]);
            button.setText(texts[z]);
            button.setData(z);
            if (z == defaultValue) button.setFocus();
            button.setLayoutData(new TableLayoutData(0,z,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,textWidth+20,SWT.DEFAULT));
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;

                result[0] = (Integer)widget.getData();
                close(dialog,true);
              }
            });
          }
        }
      }

      if ((Boolean)run(dialog,false))
      {
        return result[0];
      }
      else
      {
        return -1;
      }
    }
    else
    {
      return defaultValue;
    }
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell parentShell, String title, String message, String[] texts, String[] helpTexts, boolean[] enabled, int defaultValue)
  {
    return select(parentShell,title,message,texts,helpTexts,null,null,null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell parentShell, String title, String message, String[] texts, boolean[] enabled, int defaultValue)
  {
    return select(parentShell,title,message,texts,null,null,null,null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell parentShell, String title, String message, String[] texts, String[] helpTexts, int defaultValue)
  {
    return select(parentShell,title,message,texts,helpTexts,null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell parentShell, String title, String message, String[] texts, int defaultValue)
  {
    return select(parentShell,title,message,texts,null,null,defaultValue);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return selection
   */
  public static BitSet selectMulti(Shell parentShell, String title, String message, String[] texts, String yesText, String noText, BitSet defaultValue)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    Composite composite,subComposite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final BitSet result = new BitSet(texts.length);

      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        label = new Label(composite,SWT.LEFT);
        label.setImage(IMAGE);
        label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

        // message
        label = new Label(composite,SWT.LEFT|SWT.WRAP);
        label.setText(message);
        label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0,4));

        // checkboxes
        subComposite = new Composite(composite,SWT.NONE);
        subComposite.setLayout(new TableLayout(0.0,1.0));
        subComposite.setLayoutData(new TableLayoutData(1,1,TableLayoutData.NSWE));
        {
          int value = 0;
          for (String text : texts)
          {
            button = new Button(subComposite,SWT.CHECK);
            button.setText(text);
            button.setData(value);
            if ((defaultValue != null) && defaultValue.get(value)) button.setFocus();
            button.setLayoutData(new TableLayoutData(value,0,TableLayoutData.W));
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                int    value  = (Integer)widget.getData();

                result.set(value,widget.getSelection());
              }
            });

            value++;
          }
        }
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(2,0,TableLayoutData.WE,0,0,4));
      {
        button = new Button(composite,SWT.CENTER);
        button.setText(yesText);
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,true);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(noText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,false);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      if ((Boolean)run(dialog,defaultValue))
      {
        return result;
      }
      else
      {
        return null;
      }
    }
    else
    {
      return defaultValue;
    }
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param yesText yes-text
   * @param noText no-text
   * @return selection
   */
  public static BitSet selectMulti(Shell parentShell, String title, String message, String[] texts, String yesText, String noText)
  {
    return selectMulti(parentShell,title,message,texts,yesText,noText,null);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @return selection
   */
  public static BitSet selectMulti(Shell parentShell, String title, String message, String[] texts)
  {
    return selectMulti(parentShell,title,message,texts,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @param text1 text
   * @param text2 text (can be null)
   * @param okText OK button text
   * @param CancelText cancel button text
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String text1, final String text2, String okText, String cancelText)
  {
    int       row;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));

      // password
      final Text   widgetPassword1,widgetPassword2;
      final Button widgetOkButton;
      row = 0;
      if (message != null)
      {
        label = new Label(dialog,SWT.LEFT);
        label.setText(message);
        label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W));
        row++;
      }
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
      composite.setLayoutData(new TableLayoutData(row+0,0,TableLayoutData.WE));
      {
        label = new Label(composite,SWT.LEFT);
        label.setText(text1);
        label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

        widgetPassword1 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
        widgetPassword1.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));

        if (text2 != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text2);
          label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));

          widgetPassword2 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
          widgetPassword2.setLayoutData(new TableLayoutData(1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        }
        else
        {
          widgetPassword2 = null;
        }
      }
      row++;

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String password1 = widgetPassword1.getText();
            if (text2 != null)
            {
              String password2 = widgetPassword2.getText();
              if (password1.equals(password2))
              {
                close(dialog,password1);
              }
            }
            else
            {
              close(dialog,password1);
            }
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetPassword1.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          if (text2 != null)
          {
            widgetPassword2.forceFocus();
          }
          else
          {
            widgetOkButton.forceFocus();
          }
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      if (text2 != null)
      {
        widgetPassword2.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            widgetOkButton.forceFocus();
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      return (String)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text1 text
   * @param text2 text (can be null)
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String text1, String text2)
  {
    return password(parentShell,title,message,text1,text2,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String text)
  {
    return password(parentShell,title,message,text,null,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String text)
  {
    return password(parentShell,title,null,text,null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title)
  {
    return password(parentShell,title,"Password:");
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type SWT.OPEN or SWT.SAVE
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return file name or null
   */
  private static String file(Shell parentShell, int type, String title, String oldFileName, String[] fileExtensions, String defaultFileExtension)
  {
    File oldFile = (oldFileName != null)?new File(oldFileName):null;

    FileDialog dialog = new FileDialog(parentShell,type);
    dialog.setText(title);
    if (oldFile != null)
    {
      dialog.setFilterPath(oldFile.getParent());
      dialog.setFileName(oldFile.getName());
    }
    dialog.setOverwrite(false);
    if (fileExtensions != null)
    {
      assert (fileExtensions.length % 2) == 0;

      String[] fileExtensionNames = new String[fileExtensions.length/2];
      for (int z = 0; z < fileExtensions.length/2; z++)
      {
        fileExtensionNames[z] = fileExtensions[z*2+0]+" ("+fileExtensions[z*2+1]+")";
      }
      String[] fileExtensionPatterns = new String[(fileExtensions.length+1)/2];
      int fileExtensionIndex = 0;
      for (int z = 0; z < fileExtensions.length/2; z++)
      {
        fileExtensionPatterns[z] = fileExtensions[z*2+1];
        if ((defaultFileExtension != null) && defaultFileExtension.equalsIgnoreCase(fileExtensions[z*2+1])) fileExtensionIndex = z;
      }
      dialog.setFilterNames(fileExtensionNames);
      dialog.setFilterExtensions(fileExtensionPatterns);
      dialog.setFilterIndex(fileExtensionIndex);
    }

    String fileName = dialog.open();
    if (fileName != null)
    {
      // convert to relative path (when possible)
      try
      {
        String canonicalFileName = new File(fileName).getCanonicalPath();
        if (oldFile != null)
        {
          if (canonicalFileName.startsWith(oldFile.getCanonicalPath()))
          {
            fileName = canonicalFileName.substring(oldFile.getCanonicalPath().length()+1);
          }
        }
      }
      catch (IOException exception)
      {
      }
    }

    return fileName;
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return file name or null
   */
  public static String fileOpen(Shell parentShell, String title, String fileName, String[] fileExtensions, String defaultFileExtension)
  {
    return file(parentShell,SWT.OPEN,title,fileName,fileExtensions,defaultFileExtension);
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  public static String fileOpen(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return fileOpen(parentShell,title,fileName,fileExtensions,null);
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @return file name or null
   */
  public static String fileOpen(Shell parentShell, String title, String fileName)
  {
    return fileOpen(parentShell,title,fileName,null);
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @return file name or null
   */
  public static String fileOpen(Shell parentShell, String title)
  {
    return fileOpen(parentShell,title,null);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return file name or null
   */
  public static String fileSave(Shell parentShell, String title, String fileName, String[] fileExtensions, String defaultFileExtension)
  {
    return file(parentShell,SWT.SAVE,title,fileName,fileExtensions,defaultFileExtension);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  public static String fileSave(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return fileSave(parentShell,title,fileName,fileExtensions,null);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param defaultFileExtension default file extension pattern or null
   * @return file name or null
   */
  public static String fileSave(Shell parentShell, String title, String fileName, String defaultFileExtension)
  {
    return fileSave(parentShell,title,fileName,null,defaultFileExtension);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @return file name or null
   */
  public static String fileSave(Shell parentShell, String title, String fileName)
  {
    return fileSave(parentShell,title,fileName,(String)null);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @return file name or null
   */
  public static String fileSave(Shell parentShell, String title)
  {
    return fileSave(parentShell,title,null);
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @param text text or null
   * @param pathName path name or null
   * @return directory name or null
   */
  public static String directory(Shell parentShell, String title, String text, String pathName)
  {
    DirectoryDialog dialog = new DirectoryDialog(parentShell);
    dialog.setText(title);
    if (text != null) dialog.setMessage(text);
    if (pathName != null)
    {
      dialog.setFilterPath(pathName);
    }

    return dialog.open();
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @param pathName path name or null
   * @return directory name or null
   */
  public static String directory(Shell parentShell, String title, String pathName)
  {
    return directory(parentShell,title,null,pathName);
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @return directory name or null
   */
  public static String directory(Shell parentShell, String title)
  {
    return directory(parentShell,title,null);
  }

  /** simple path dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return path or null on cancel
   */
  public static String path(Shell parentShell, String title, String text, String value, String okText, String cancelText, String toolTipText)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"directory.png");

    int             row;
    Composite       composite;
    Label           label;
    Button          button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      final Text   widgetPath;
      final Button widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        int column = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W));
          column++;
        }
        widgetPath = new Text(composite,SWT.LEFT|SWT.BORDER);
        if (value != null)
        {
          widgetPath.setText(value);
          widgetPath.setSelection(value.length(),value.length());
        }
        widgetPath.setLayoutData(new TableLayoutData(0,column,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        if (toolTipText != null) widgetPath.setToolTipText(toolTipText);
        column++;

        button = new Button(composite,SWT.CENTER);
        button.setImage(IMAGE);
        button.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String path = directory(dialog,"Select path");
            if (path != null)
            {
              widgetPath.setText(path);
            }
          }
        });
        column++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetPath.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetPath.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetPath.setFocus();
      return (String)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** simple path dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return path or null on cancel
   */
  public static String path(Shell parentShell, String title, String text, String value, String okText, String cancelText)
  {
    return path(parentShell,title,text,value,okText,cancelText,null);
  }

  /** simple path dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @return path or null on cancel
   */
  public static String path(Shell parentShell, String title, String text, String value)
  {
    return path(parentShell,title,text,value,"OK","Cancel");
  }

  /** simple path dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @return path or null on cancel
   */
  public static String path(Shell parentShell, String title, String text)
  {
    return path(parentShell,title,text,null);
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String string(Shell parentShell, String title, String text, String value, String okText, String cancelText, String toolTipText)
  {
    int             row;
    Composite       composite;
    Label           label;
    Button          button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // string
      final Text   widgetString;
      final Button widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        int column = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W));
          column++;
        }
        widgetString = new Text(composite,SWT.LEFT|SWT.BORDER);
        if (value != null)
        {
          widgetString.setText(value);
          widgetString.setSelection(value.length(),value.length());
        }
        widgetString.setLayoutData(new TableLayoutData(0,column,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        if (toolTipText != null) widgetString.setToolTipText(toolTipText);
        column++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetString.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetString.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetString.setFocus();
      return (String)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return string or null on cancel
   */
  public static String string(Shell parentShell, String title, String text, String value, String okText, String cancelText)
  {
    return string(parentShell,title,text,value,okText,cancelText,null);
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @return string or null on cancel
   */
  public static String string(Shell parentShell, String title, String text, String value)
  {
    return string(parentShell,title,text,value,"Save","Cancel");
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @return string or null on cancel
   */
  public static String string(Shell parentShell, String title, String text)
  {
    return string(parentShell,title,text,"");
  }

  /** simple integer dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit
   * @param minValue,maxValue min./max. value
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return value or null on cancel
   */
  public static Integer integer(Shell parentShell, String title, String text, int value, int minValue, int maxValue, String okText, String cancelText, String toolTipText)
  {
    int             row;
    Composite       composite;
    Label           label;
    Button          button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,100,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // string
      final Spinner widgetInteger;
      final Button  widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        int column = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W));
          column++;
        }
        widgetInteger = new Spinner(composite,SWT.RIGHT|SWT.BORDER);
        widgetInteger.setMinimum(minValue);
        widgetInteger.setMaximum(maxValue);
        widgetInteger.setSelection(value);
        widgetInteger.setLayoutData(new TableLayoutData(0,column,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        if (toolTipText != null) widgetInteger.setToolTipText(toolTipText);
        column++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,new Integer(widgetInteger.getSelection()));
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetInteger.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetOkButton.setFocus();
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetInteger.setFocus();
      return (Integer)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** simple integer dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit
   * @param minValue,maxValue min./max. value
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return value or null on cancel
   */
  public static Integer integer(Shell parentShell, String title, String text, int value, int minValue, int maxValue, String okText, String cancelText)
  {
    return integer(parentShell,title,text,value,minValue,maxValue,okText,cancelText,null);
  }

  /** simple integer dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit
   * @param minValue,maxValue min./max. value
   * @return value or null on cancel
   */
  public static Integer integer(Shell parentShell, String title, String text, int value, int minValue, int maxValue)
  {
    return integer(parentShell,title,text,value,minValue,maxValue,"OK","Cancel");
  }

  /** open simple busy dialog
   * @param parentShell parent shell
   * @param title title text
   * @param message info message
   * @param abortButton true for abort-button, false otherwise
   * @return simple busy dialog
   */
  public static SimpleBusyDialog openSimpleBusy(Shell parentShell, String title, String message, boolean abortButton)
  {
    return new SimpleBusyDialog(parentShell,title,message,abortButton);
  }

  /** open simple busy dialog
   * @param parentShell parent shell
   * @param title title text
   * @param message info message
   * @return simple busy dialog
   */
  public static SimpleBusyDialog openSimpleBusy(Shell parentShell, String title, String message)
  {
    return openSimpleBusy(parentShell,title,message,false);
  }

  /** open simple busy dialog
   * @param parentShell parent shell
   * @param message info message
   * @return simple busy dialog
   */
  public static SimpleBusyDialog openSimpleBusy(Shell parentShell, String message, boolean abortButton)
  {
    return openSimpleBusy(parentShell,"Busy",message,abortButton);
  }

  /** open simple busy dialog
   * @param parentShell parent shell
   * @param message info message
   * @return simple busy dialog
   */
  public static SimpleBusyDialog openSimpleBusy(Shell parentShell, String message)
  {
    return openSimpleBusy(parentShell,message,false);
  }

  /** close simple busy dialog
   * @param simpleBusyDialog busy dialog
   */
  public static void closeSimpleBusy(SimpleBusyDialog simpleBusyDialog)
  {
    simpleBusyDialog.close();
  }

  /** open busy dialog
   * @param parentShell parent shell
   * @param title title text
   * @param message info message
   * @return busy dialog
   */
  public static BusyDialog openBusy(Shell parentShell, String title, String message)
  {
    return new BusyDialog(parentShell,title,message);
  }

  /** open busy dialog
   * @param parentShell parent shell
   * @param message info message
   * @return busy dialog
   */
  public static BusyDialog openBusy(Shell parentShell, String message)
  {
    return openBusy(parentShell,"Busy",message);
  }

  /** close busy dialog
   * @param busyDialog busy dialog
   */
  public static void closeBusy(BusyDialog busyDialog)
  {
    busyDialog.close();
  }

/// NYI not complete
  /** progress dialog
   * @param parentShell parent shell
   * @param title text
   * @param message info message
   * @return simple progress dialog
   */
  public static SimpleProgressDialog openSimpleProgress(Shell parentShell, String title, String message)
  {
    return new SimpleProgressDialog(parentShell,title,message);
  }

  /** progress dialog
   * @param parentShell parent shell
   * @param message info message
   * @return simple progress dialog
   */
  public static SimpleProgressDialog openSimpleProgress(Shell parentShell, String message)
  {
    return openSimpleProgress(parentShell,"Progress",message);
  }

  /** close simple progress dialog
   * @param simpleProgressDialog simple progress dialog
   */
  public static void closeSimpleProgress(SimpleProgressDialog simpleProgressDialog)
  {
    simpleProgressDialog.close();
  }
}

/* end of file */
