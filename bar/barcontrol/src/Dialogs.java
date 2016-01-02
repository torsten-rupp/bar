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
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.text.MessageFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSource;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.DropTarget;
import org.eclipse.swt.dnd.DropTargetAdapter;
import org.eclipse.swt.dnd.DropTargetEvent;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
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
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Monitor;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Slider;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;

import org.xnap.commons.i18n.I18n;
import org.xnap.commons.i18n.I18nFactory;

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
        button.setText(Dialogs.tr("Abort"));
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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

/** list values runnable
 */
abstract class ListRunnable
{
  /** get list values
   * @param list value
   */
  abstract public String[] getValues();

  /** get selected value
   * @param list value
   */
  abstract public String getSelection();
}

/** boolean field updater
 */
class BooleanFieldUpdater
{
  private Field  field;
  private Object object;

  /** create boolean field updater
   * @param clazz class with boolean or Boolean field
   * @param object object instance
   * @param fieldName field name
   */
  BooleanFieldUpdater(Class clazz, Object object, String fieldName)
  {
    try
    {
      this.field  = clazz.getDeclaredField(fieldName);
      this.object = object;
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

/** list directory
 */
abstract class ListDirectory
{
  /** get shortcut files
   * @return shortcut files
   */
  public File[] getShortcuts()
  {
    return null;
  }

  /** set shortcut files
   * @param shortcuts shortcut files
   */
  public void setShortcuts(File shortcuts[])
  {
  }

  /** open list files in directory
   * @param pathName path name
   * @return true iff open
   */
  abstract public boolean open(String pathName);

  /** close list files in directory
   */
  abstract public void close();

  /** get next entry in directory
   * @return entry
   */
  abstract public File getNext();

  /** check if directory
   * @param file file to check
   * @return true if file is directory
   */
  public boolean isDirectory(File file)
  {
    return file.isDirectory();
  }

  /** check if file
   * @param file file to check
   * @return true if file is file
   */
  public boolean isFile(File file)
  {
    return file.isFile();
  }

  /** check if hidden
   * @param file file to check
   * @return true if file is hidden
   */
  public boolean isHidden(File file)
  {
    return file.isHidden();
  }

  /** check if exists
   * @param file file to check
   * @return true if file exists
   */
  public boolean exists(File file)
  {
    return file.exists();
  }
}

/** dialog
 */
class Dialogs
{
  // --------------------------- constants --------------------------------
  public enum FileDialogTypes
  {
    OPEN,
    SAVE,
    DIRECTORY
  };

  // --------------------------- variables --------------------------------
  private static I18n  i18n;

  private static Point fileGeometry = new Point(600,400);

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** init internationalization
   * @param i18n internationlization instance
   */
  public static void init(I18n i18n)
  {
    Dialogs.i18n = i18n;
  }

  /** get internationalized text
   * @param text text
   * @param arguments text
   * @return internationalized text
   */
  public static String tr(String text, Object... arguments)
  {
    if (i18n != null)
    {
      return i18n.tr(text,arguments);
    }
    else
    {
      return MessageFormat.format(text,arguments);
    }
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
   * @param object object instance
   * @param fieldName field name
   * @return boolean field updater
   */
  public static BooleanFieldUpdater booleanFieldUpdater(Class clazz, Object object, String fieldName)
  {
    return new BooleanFieldUpdater(clazz,object,fieldName);
  }

  /** create boolean field updater
   * @param clazz class with boolean or Boolean field
   * @param fieldName field name
   * @return boolean field updater
   */
  public static BooleanFieldUpdater booleanFieldUpdater(Class clazz, String fieldName)
  {
    return booleanFieldUpdater(clazz,null,fieldName);
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
            widgetShowAgain.setText(Dialogs.tr("show again"));
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
          button.setText(Dialogs.tr("Close"));
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
    info(parentShell,Dialogs.tr("Information"),showAgainFieldFlag,message);
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
        final Shell dialog = openModal(parentShell,Dialogs.tr("Error"),300,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(new double[]{0.0,0.0,1.0,0.0},new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          int row = 0;

          label = new Label(composite,SWT.LEFT);
          label.setImage(IMAGE);
          label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W,0,0,10));
          text = new Text(composite,SWT.LEFT|SWT.WRAP|SWT.READ_ONLY);
          text.setText(message);
          text.setBackground(composite.getBackground());
          text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
          text.addMouseListener(new MouseListener()
          {
            public void mouseDoubleClick(MouseEvent mouseEvent)
            {
              Text widget = (Text)mouseEvent.widget;

              widget.setSelection(0,widget.getText().length());
            }
            public void mouseDown(MouseEvent mouseEvent)
            {
            }
            public void mouseUp(MouseEvent mouseEvent)
            {
            }
          });
          row++;

          if (extendedMessage != null)
          {
            label = new Label(composite,SWT.LEFT);
            label.setText(Dialogs.tr("Extended error:"));
            label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
            row++;
            text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.READ_ONLY);
            text.setText(StringUtils.join(extendedMessage,text.DELIMITER));
            text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,100));
            row++;
          }

          if (showAgainFieldFlag != null)
          {
            widgetShowAgain = new Button(composite,SWT.CHECK);
            widgetShowAgain.setText(Dialogs.tr("show again"));
            widgetShowAgain.setSelection(true);
            widgetShowAgain.setLayoutData(new TableLayoutData(row,1,TableLayoutData.W));
            row++;
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
          button.setText(Dialogs.tr("Close"));
          button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, java.util.List<String> extendedMessage, String message)
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
  public static void error(Shell parentShell, BooleanFieldUpdater showAgainFieldFlag, java.util.List<String> extendedMessage, String format, Object... arguments)
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
  public static void error(Shell parentShell, java.util.List<String> extendedMessage, String format, Object... arguments)
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
        final Shell dialog = open(parentShell,Dialogs.tr("Warning"),200,70);
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
            widgetShowAgain.setText(Dialogs.tr("show again"));
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
          button.setText(Dialogs.tr("Close"));
          button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,100,SWT.DEFAULT));
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
            widgetShowAgain.setText(Dialogs.tr("show again"));
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
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
          button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
                            defaultValue,
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
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, Image image, String message, String yesText)
  {
    return confirm(parentShell,title,null,image,message,yesText,Dialogs.tr("Cancel"));
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
   * @param message confirmation message
   * @param yesText yes-text
   * @return value
   */
  public static boolean confirm(Shell parentShell, String title, String message, String yesText)
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message,yesText,Dialogs.tr("Cancel"));
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
    return confirm(parentShell,title,showAgainFieldFlag,message,Dialogs.tr("Yes"),Dialogs.tr("No"),defaultValue);
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
    return confirm(parentShell,title,showAgainFieldFlag,message,Dialogs.tr("Yes"),Dialogs.tr("No"),false);
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
    return confirm(parentShell,Dialogs.tr("Confirmation"),showAgainFieldFlag,message,defaultValue);
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
            button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
            button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
    return selectMulti(parentShell,title,message,texts,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display (can be null)
   * @param userName user name to display (can be null)
   * @param text1 text (can be null)
   * @param text2 text (can be null)
   * @param okText OK button text
   * @param CancelText cancel button text
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String userName, String text1, final String text2, String okText, String cancelText)
  {
    int       row0,row1;
    Composite composite;
    Label     label;
    Text      text;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));

      // password
      final Text   widgetPassword1,widgetPassword2;
      final Button widgetOkButton;
      row0 = 0;
      if (message != null)
      {
        label = new Label(dialog,SWT.LEFT);
        label.setText(message);
        label.setLayoutData(new TableLayoutData(row0,0,TableLayoutData.W));
        row0++;
      }
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
      composite.setLayoutData(new TableLayoutData(row0,0,TableLayoutData.WE));
      {
        row1 = 0;
        if (userName != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(Dialogs.tr("Name")+":");
          label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

          text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.READ_ONLY);
          text.setBackground(composite.getBackground());
          text.setText(userName);
          text.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));

          row1++;
        }

        label = new Label(composite,SWT.LEFT);
        label.setText((text1 != null) ? text1 : Dialogs.tr("Password")+":");
        label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

        widgetPassword1 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
        widgetPassword1.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        row1++;

        if (text2 != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text2);
          label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

          widgetPassword2 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
          widgetPassword2.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));

          row1++;
        }
        else
        {
          widgetPassword2 = null;
        }
      }
      row0++;

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(row0,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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

      widgetPassword1.setFocus();
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
   * @param message message to display
   * @param userName user name to display (can be null)
   * @param text1 text
   * @param text2 text (can be null)
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String userName, String text1, String text2)
  {
    return password(parentShell,title,message,userName,text1,text2,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @param text1 text
   * @param text2 text (can be null)
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String text1, String text2)
  {
    return password(parentShell,title,message,null,text1,text2);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message, String text)
  {
    return password(parentShell,title,message,text,null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title, String message)
  {
    return password(parentShell,title,message,null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @return password or null on cancel
   */
  public static String password(Shell parentShell, String title)
  {
    return password(parentShell,title,null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @return file name or null
   */
  public static String file(Shell               parentShell,
                            FileDialogTypes     type,
                            String              title,
                            String              oldFileName,
                            final String[]      fileExtensions,
                            String              defaultFileExtension,
                            final ListDirectory listDirectory
                           )
  {
    /** dialog data
     */
    class Data
    {
      boolean showHidden;
    }

    /** file comparator
    */
    class FileComparator implements Comparator<File>
    {
      // Note: enum in inner classes are not possible in Java, thus use the old way...
      private final static int SORTMODE_NAME     = 0;
      private final static int SORTMODE_TYPE     = 1;
      private final static int SORTMODE_MODIFIED = 2;
      private final static int SORTMODE_SIZE     = 3;

      private int sortMode;

      /** create file data comparator
       * @param sortMode sort mode
       */
      FileComparator(int sortMode)
      {
        this.sortMode = sortMode;
      }

      /** set sort mode
       * @param sortMode sort mode
       */
      public void setSortMode(int sortMode)
      {
        this.sortMode = sortMode;
      }

      /** compare file tree data
       * @param file1, file2 file data to compare
       * @return -1 iff file1 < file2,
                  0 iff file1 = file2,
                  1 iff file1 > file2
       */
      public int compare(File file1, File file2)
      {
//System.out.println(String.format("file1=%s file2=%s",file1,file2));
        switch (sortMode)
        {
          case SORTMODE_NAME:
            return file1.getName().compareTo(file2.getName());
          case SORTMODE_TYPE:
            if      (file1.isDirectory())
            {
              if   (file2.isDirectory()) return 0;
              else                       return 1;
            }
            else
            {
              if   (file2.isDirectory()) return -1;
              else                       return 0;
            }
          case SORTMODE_MODIFIED:
            if      (file1.lastModified() < file2.lastModified()) return -1;
            else if (file1.lastModified() > file2.lastModified()) return  1;
            else                                                  return  0;
          case SORTMODE_SIZE:
            if      (file1.length() < file2.length()) return -1;
            else if (file1.length() > file2.length()) return  1;
            else                                      return  0;
          default:
            return 0;
        }
      }

      /** convert data to string
       * @return string
       */
      public String toString()
      {
        return "FileComparator {"+sortMode+"}";
      }
    }

    /** updater
    */
    class Updater
    {
      private FileComparator   fileComparator;
      private ListDirectory    listDirectory;
      private SimpleDateFormat simpleDateFormat;
      private Pattern          fileFilterPattern;
      private boolean          showHidden;
      private boolean          showFiles;

      /** create update file list
       * @param listDirectory list directory
       * @param fileComparator file comparator
       */
      public Updater(FileComparator fileComparator, ListDirectory listDirectory, boolean showFiles)
      {
        this.fileComparator    = fileComparator;
        this.listDirectory     = listDirectory;
        this.simpleDateFormat  = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        this.fileFilterPattern = null;
        this.showHidden        = false;
        this.showFiles         = showFiles;
      }

      /** update shortcut list
       * @param shortcutFileList shortcut file list
       * @param widgetShortcutList shortcut list widget
       * @param shortcutSet shortcut set
       */
      public void updateShortcutList(ArrayList<File> shortcutFileList, List widgetShortcutList, HashSet<File> shortcutSet)
      {
        HashSet<String> nameSet = new HashSet<String>();

        shortcutFileList.clear();
        for (File file : shortcutSet)
        {
          shortcutFileList.add(file);
        }
        Collections.sort(shortcutFileList,fileComparator);

        widgetShortcutList.removeAll();
        for (File file : shortcutFileList)
        {
          widgetShortcutList.add(file.getAbsolutePath());
        }
      }

      /** update file list
       * @param table table widget
       * @param path path
       * @param name name or null
       */
      public void updateFileList(Table table, String path, String name)
      {
        if (!table.isDisposed())
        {
          table.removeAll();

          if (listDirectory.open(path))
          {
            // update list
            File file;
            while ((file = listDirectory.getNext()) != null)
            {
              if (   (showHidden || !listDirectory.isHidden(file))
                  && (showFiles || !listDirectory.isFile(file))
                  && ((fileFilterPattern == null) || fileFilterPattern.matcher(file.getName()).matches())
                 )
              {
                TableItem tableItems[] = table.getItems();
                int index = 0;
                while (   (index < tableItems.length)
                       && (fileComparator.compare(file,(File)tableItems[index].getData()) > 0)
                      )
                {
                  index++;
                }

                TableItem tableItem = new TableItem(table,SWT.NONE,index);
                tableItem.setData(file);
                tableItem.setText(0,file.getName());
                if (file.isDirectory()) tableItem.setText(1,"DIR");
                else                    tableItem.setText(1,"FILE");
                tableItem.setText(2,simpleDateFormat.format(new Date(file.lastModified())));
                if (file.isDirectory()) tableItem.setText(3,"");
                else                    tableItem.setText(3,Long.toString(file.length()));
              }
            }
            listDirectory.close();

            // select name
            if (name != null)
            {
              int index = 0;
              for (TableItem tableItem : table.getItems())
              {
                if (((File)tableItem.getData()).getName().equals(name))
                {
                  table.select(index);
                  break;
                }
                index++;
              }
            }
          }
        }
      }

      /** set filter
       * @param filter glob filter string
       */
      public void setFileFilter(String fileFilter)
      {
        this.showHidden = showHidden;

        // convert glob-pattern => regular expression
        StringBuilder buffer = new StringBuilder();
        int i = 0;
        while (i < fileFilter.length())
        {
          char ch = fileFilter.charAt(i);
          switch (ch)
          {
            case '\\':
              i++;
              buffer.append(ch);
              if (i < fileFilter.length())
              {
                buffer.append(fileFilter.charAt(i));
              }
              break;
            case '*':
              buffer.append(".*");
              break;
            case '?':
              buffer.append(".");
              break;
            case '.':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '&':
              buffer.append('\\');
              buffer.append(ch);
              break;
            default:
              buffer.append(ch);
          }
          i++;
        }

        // compile regualar expression
        fileFilterPattern = Pattern.compile(buffer.toString());
      }

      /** set show hidden
       * @param showHidden true to show hidden files, too
       */
      public void setShowHidden(boolean showHidden)
      {
        this.showHidden = showHidden;
      }

      /** set show files
       * @param showFiles true to show files, too
       */
      public void setShowFiles(boolean showFiles)
      {
        this.showFiles = showFiles;
      }
    }

    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' folderUp.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_FOLDER_UP_DATA_ARRAY =
    {
      (byte)0x89,(byte)0x50,(byte)0x4e,(byte)0x47,(byte)0x0d,(byte)0x0a,(byte)0x1a,(byte)0x0a,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x0d,(byte)0x49,(byte)0x48,(byte)0x44,(byte)0x52,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x10,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x10,(byte)0x08,(byte)0x06,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x1f,(byte)0xf3,(byte)0xff,(byte)0x61,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x04,(byte)0x67,(byte)0x41,(byte)0x4d,(byte)0x41,(byte)0x00,(byte)0x00,(byte)0xaf,(byte)0xc8,
      (byte)0x37,(byte)0x05,(byte)0x8a,(byte)0xe9,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x19,(byte)0x74,
      (byte)0x45,(byte)0x58,(byte)0x74,(byte)0x53,(byte)0x6f,(byte)0x66,(byte)0x74,(byte)0x77,(byte)0x61,
      (byte)0x72,(byte)0x65,(byte)0x00,(byte)0x41,(byte)0x64,(byte)0x6f,(byte)0x62,(byte)0x65,(byte)0x20,
      (byte)0x49,(byte)0x6d,(byte)0x61,(byte)0x67,(byte)0x65,(byte)0x52,(byte)0x65,(byte)0x61,(byte)0x64,
      (byte)0x79,(byte)0x71,(byte)0xc9,(byte)0x65,(byte)0x3c,(byte)0x00,(byte)0x00,(byte)0x02,(byte)0xc6,
      (byte)0x49,(byte)0x44,(byte)0x41,(byte)0x54,(byte)0x78,(byte)0xda,(byte)0x62,(byte)0xfc,(byte)0xff,
      (byte)0xff,(byte)0x3f,(byte)0x03,(byte)0x25,(byte)0x00,(byte)0x20,(byte)0x80,(byte)0x98,(byte)0x18,
      (byte)0x28,(byte)0x04,(byte)0x00,(byte)0x01,(byte)0xc4,(byte)0x02,(byte)0x22,(byte)0x6e,(byte)0x6d,
      (byte)0x35,(byte)0xbd,(byte)0xc0,(byte)0xc1,(byte)0xa7,(byte)0xa5,(byte)0xc5,(byte)0xc8,(byte)0x88,
      (byte)0x6c,(byte)0xde,(byte)0x7f,(byte)0x86,(byte)0x9f,(byte)0x9f,(byte)0xef,(byte)0x3e,(byte)0xfe,
      (byte)0xfb,(byte)0xe7,(byte)0x83,(byte)0x25,(byte)0x90,(byte)0xf3,(byte)0x13,(byte)0x45,(byte)0x17,
      (byte)0x23,(byte)0x98,(byte)0xfc,(byte)0xaa,(byte)0xee,(byte)0x73,(byte)0xf9,(byte)0x0f,(byte)0x40,
      (byte)0x00,(byte)0x81,(byte)0x0d,(byte)0x60,(byte)0xfc,(byte)0xcf,(byte)0xa4,(byte)0x21,(byte)0x6b,
      (byte)0x33,(byte)0x9f,(byte)0x95,(byte)0x91,(byte)0x91,(byte)0x11,(byte)0xa4,(byte)0x0f,(byte)0xac,
      (byte)0x99,(byte)0x01,(byte)0xc8,(byte)0xfe,(byte)0xf3,(byte)0xe3,(byte)0x8d,(byte)0xd2,(byte)0xcb,
      (byte)0x0b,(byte)0x8d,(byte)0x8f,(byte)0xff,(byte)0x43,(byte)0x45,(byte)0x41,(byte)0x80,(byte)0x99,
      (byte)0x8d,(byte)0xef,(byte)0xff,(byte)0xff,(byte)0xbf,(byte)0xbf,(byte)0x5f,(byte)0x7d,(byte)0x79,
      (byte)0xbe,(byte)0xaf,(byte)0x14,(byte)0xc8,(byte)0x5d,(byte)0x05,(byte)0x10,(byte)0x40,(byte)0x60,
      (byte)0x03,(byte)0x18,(byte)0xfe,(byte)0xfc,(byte)0xff,(byte)0xc1,(byte)0xf0,(byte)0xef,(byte)0x17,
      (byte)0xfb,(byte)0xcf,(byte)0x17,(byte)0xd3,(byte)0x19,(byte)0xfe,(byte)0xfe,(byte)0x62,(byte)0x66,
      (byte)0x60,(byte)0x60,(byte)0xe2,(byte)0x03,(byte)0xea,(byte)0xe7,(byte)0x65,(byte)0xe0,(byte)0x90,
      (byte)0x72,(byte)0x66,(byte)0x90,(byte)0x32,(byte)0xeb,(byte)0x64,(byte)0x63,(byte)0x00,(byte)0x87,
      (byte)0xd3,(byte)0x3f,(byte)0xa0,(byte)0x31,(byte)0xff,(byte)0xc0,(byte)0x86,(byte)0x33,(byte)0x32,
      (byte)0xf1,(byte)0x48,(byte)0xdc,(byte)0xde,(byte)0x64,(byte)0x53,(byte)0x07,(byte)0x32,(byte)0x00,
      (byte)0x20,(byte)0x80,(byte)0x20,(byte)0x06,(byte)0xfc,(byte)0x66,(byte)0x64,(byte)0xfa,(byte)0xff,
      (byte)0xef,(byte)0x17,(byte)0x50,(byte)0x33,(byte)0x50,(byte)0xed,(byte)0xbf,(byte)0xff,(byte)0x0c,
      (byte)0xff,(byte)0xbe,(byte)0x3f,(byte)0x62,(byte)0xf8,(byte)0xfd,(byte)0xf5,(byte)0x25,(byte)0xc3,
      (byte)0x97,(byte)0x57,(byte)0x07,(byte)0x18,(byte)0x58,(byte)0x39,(byte)0x24,(byte)0x81,(byte)0x7a,
      (byte)0x7e,(byte)0x03,(byte)0xf5,(byte)0xff,(byte)0x02,(byte)0x9a,(byte)0xf3,(byte)0x07,(byte)0x64,
      (byte)0x1b,(byte)0x03,(byte)0xaf,(byte)0x46,(byte)0x39,(byte)0x48,(byte)0xa1,(byte)0x3c,(byte)0x48,
      (byte)0x2b,(byte)0x40,(byte)0x00,(byte)0x41,(byte)0x0c,(byte)0x60,(byte)0xfc,(byte)0xcf,(byte)0xf1,
      (byte)0xef,(byte)0xcf,(byte)0x77,(byte)0x86,(byte)0x5f,(byte)0xdf,(byte)0x80,(byte)0x6a,(byte)0x7f,
      (byte)0x7d,(byte)0x65,(byte)0xf8,(byte)0xfd,(byte)0xf9,(byte)0x13,(byte)0x50,(byte)0x51,(byte)0x3c,
      (byte)0x03,(byte)0x87,(byte)0xb0,(byte)0x26,(byte)0xd4,(byte)0xc3,(byte)0x30,(byte)0x17,(byte)0xfc,
      (byte)0x87,(byte)0xb8,(byte)0x80,(byte)0x45,(byte)0x80,(byte)0x81,(byte)0xe1,(byte)0x2f,(byte)0x13,
      (byte)0x58,(byte)0x2f,(byte)0x40,(byte)0x00,(byte)0x41,(byte)0x5d,(byte)0xc0,(byte)0xc4,(byte)0xc0,
      (byte)0xc2,(byte)0x29,(byte)0xc6,(byte)0x20,(byte)0xa0,(byte)0x99,(byte)0x02,(byte)0x57,(byte)0x08,
      (byte)0x74,(byte)0x07,(byte)0xd0,(byte)0xd2,(byte)0xa7,(byte)0x0c,(byte)0x7f,(byte)0x3e,(byte)0xec,
      (byte)0x61,(byte)0xf8,(byte)0xff,(byte)0xf7,(byte)0x23,(byte)0x43,(byte)0xfd,(byte)0xd1,(byte)0xed,
      (byte)0x0c,(byte)0xef,(byte)0xbf,(byte)0x7f,(byte)0x65,(byte)0xe8,(byte)0xb7,(byte)0xd4,(byte)0x66,
      (byte)0xe0,(byte)0x90,(byte)0xa9,(byte)0x06,(byte)0x06,(byte)0x2b,(byte)0x24,(byte)0x58,(byte)0x00,
      (byte)0x02,(byte)0x08,(byte)0x6a,(byte)0x00,(byte)0x30,(byte)0x35,(byte)0xfc,(byte)0x05,(byte)0x06,
      (byte)0xc3,(byte)0xb7,(byte)0x4b,(byte)0x60,(byte)0xc5,(byte)0xff,(byte)0xff,(byte)0x7e,(byte)0x66,
      (byte)0xf8,(byte)0xff,(byte)0xe7,(byte)0x03,(byte)0xc3,(byte)0xbf,(byte)0xdf,(byte)0xef,(byte)0xc1,
      (byte)0x7c,(byte)0xa0,(byte)0x29,(byte)0x0c,(byte)0x3f,(byte)0xfe,(byte)0xfc,(byte)0x60,(byte)0x90,
      (byte)0x11,(byte)0xd1,(byte)0x64,(byte)0x48,(byte)0x3e,(byte)0x70,(byte)0x86,(byte)0x61,(byte)0x69,
      (byte)0x34,(byte)0x30,(byte)0x52,(byte)0x7e,(byte)0x43,(byte)0x02,(byte)0x15,(byte)0x20,(byte)0x80,
      (byte)0x20,(byte)0x06,(byte)0xfc,(byte)0x04,(byte)0xda,(byte)0xf9,(byte)0xef,(byte)0x0f,(byte)0x50,
      (byte)0xc3,(byte)0x6b,(byte)0xa0,(byte)0xc6,(byte)0x8f,(byte)0x60,(byte)0x4d,(byte)0xe5,(byte)0xfb,
      (byte)0x57,(byte)0x80,(byte)0xfd,(byte)0xfe,(byte)0x0b,(byte)0x28,(byte)0xfe,(byte)0xeb,(byte)0xcf,
      (byte)0x1f,(byte)0x06,(byte)0x49,(byte)0x41,(byte)0x55,(byte)0x06,(byte)0x4d,(byte)0x09,(byte)0x73,
      (byte)0x86,(byte)0xcf,(byte)0x3f,(byte)0xbe,(byte)0x32,(byte)0xf8,(byte)0xcc,(byte)0xf3,(byte)0x62,
      (byte)0x98,(byte)0xc8,(byte)0x2e,(byte)0x0d,(byte)0xd6,(byte)0x0a,(byte)0x10,(byte)0x40,(byte)0x10,
      (byte)0x03,(byte)0x7e,(byte)0xfd,(byte)0xff,(byte)0xcf,(byte)0xc8,(byte)0xf0,(byte)0x17,(byte)0xa8,
      (byte)0xf9,(byte)0x1d,(byte)0xd8,(byte)0x66,(byte)0x10,(byte)0xfe,(byte)0x03,(byte)0x0c,(byte)0x34,
      (byte)0x57,(byte)0xed,(byte)0x78,(byte)0x86,(byte)0xbf,(byte)0xc0,(byte)0x90,(byte)0xff,(byte)0xfb,
      (byte)0xef,(byte)0x2f,(byte)0xd0,(byte)0x43,(byte)0xff,(byte)0x19,(byte)0x9e,(byte)0x7d,(byte)0x7c,
      (byte)0xcc,(byte)0xa0,(byte)0x2b,(byte)0x63,(byte)0xcd,(byte)0xf0,(byte)0xe5,(byte)0xd7,(byte)0x77,
      (byte)0x86,(byte)0xf8,(byte)0x47,(byte)0x87,(byte)0x38,(byte)0xbe,(byte)0xb6,(byte)0x30,(byte)0x31,
      (byte)0x02,(byte)0x04,(byte)0x10,(byte)0xd8,(byte)0x80,(byte)0xff,(byte)0xbf,(byte)0xfe,(byte)0xbd,
      (byte)0xff,(byte)0xf3,(byte)0xed,(byte)0x2d,(byte)0xd7,(byte)0xef,(byte)0x2f,(byte)0x6c,(byte)0xbc,
      (byte)0x0c,(byte)0xff,(byte)0x78,(byte)0x99,(byte)0xfe,(byte)0xff,(byte)0x67,(byte)0x05,(byte)0x3a,
      (byte)0xf9,(byte)0x17,(byte)0x58,(byte)0xf3,(byte)0xc3,(byte)0xb7,(byte)0xb7,(byte)0x19,(byte)0x7e,
      (byte)0x03,(byte)0x5d,(byte)0xf1,(byte)0xe7,(byte)0xdf,(byte)0x6f,(byte)0x86,(byte)0xdf,(byte)0x7f,
      (byte)0x7f,(byte)0x33,(byte)0x7c,(byte)0xfa,(byte)0xf9,(byte)0x99,(byte)0xc1,(byte)0x40,(byte)0xd6,
      (byte)0x96,(byte)0xe1,(byte)0xcb,(byte)0xef,(byte)0x6f,(byte)0x0c,(byte)0x27,(byte)0xef,(byte)0x1f,
      (byte)0xf9,(byte)0x01,(byte)0x10,(byte)0x40,(byte)0x10,(byte)0x03,(byte)0x7e,(byte)0xfc,(byte)0xec,
      (byte)0xba,(byte)0x37,(byte)0x23,(byte)0xd0,(byte)0x0c,(byte)0x68,(byte)0x89,(byte)0x0b,(byte)0x30,
      (byte)0xd0,(byte)0x79,(byte)0x41,(byte)0x62,(byte)0x3f,(byte)0x84,(byte)0xff,(byte)0x70,(byte)0xfc,
      (byte)0xf9,(byte)0xfb,(byte)0x87,(byte)0x41,(byte)0x9c,(byte)0x4f,(byte)0x0e,(byte)0xa8,(byte)0xf9,
      (byte)0x2f,(byte)0xd8,(byte)0xb0,(byte)0xe7,(byte)0x1f,(byte)0x1f,(byte)0x31,(byte)0x08,(byte)0xf1,
      (byte)0x48,(byte)0x30,(byte)0x9c,(byte)0x7b,(byte)0x7c,(byte)0x94,(byte)0xe1,(byte)0xfa,(byte)0xbd,
      (byte)0x13,(byte)0xef,(byte)0xbf,(byte)0xfd,(byte)0x66,(byte)0x90,(byte)0x06,(byte)0x08,(byte)0x20,
      (byte)0x46,(byte)0x5c,(byte)0x99,(byte)0x29,(byte)0x68,(byte)0x86,(byte)0xd2,(byte)0xdf,(byte)0x1f,
      (byte)0xbf,(byte)0x7e,(byte)0x01,(byte)0xc3,(byte)0xe0,(byte)0x17,(byte)0xd8,(byte)0x35,(byte)0xca,
      (byte)0x22,(byte)0xda,(byte)0x4c,(byte)0x16,(byte)0x4a,(byte)0x6e,(byte)0x0c,(byte)0x67,(byte)0x1e,
      (byte)0x1d,(byte)0x63,(byte)0xd8,(byte)0x7b,(byte)0x6b,(byte)0xd7,(byte)0xa5,(byte)0x5f,(byte)0x7f,
      (byte)0x18,(byte)0xcc,(byte)0x9e,(byte)0xb5,(byte)0xfd,(byte)0xff,(byte)0x09,(byte)0x10,(byte)0x40,
      (byte)0x8c,(byte)0xc4,(byte)0xe6,(byte)0x46,(byte)0x9d,(byte)0x16,(byte)0xa6,(byte)0xd7,(byte)0x26,
      (byte)0x72,(byte)0x4e,(byte)0x22,(byte)0x07,(byte)0x6e,(byte)0xed,(byte)0x3d,(byte)0xfb,(byte)0xeb,
      (byte)0x1f,(byte)0x83,(byte)0x15,(byte)0x50,(byte)0xf3,(byte)0x2f,(byte)0x90,(byte)0x38,(byte)0x40,
      (byte)0x00,(byte)0x11,(byte)0x6d,(byte)0x80,(byte)0x52,(byte)0x1d,(byte)0xe3,(byte)0xeb,(byte)0xbf,
      (byte)0xff,(byte)0x98,(byte)0x45,(byte)0xbe,(byte)0xff,(byte)0xfb,(byte)0xcb,(byte)0xfa,(byte)0xaa,
      (byte)0x0d,(byte)0x9c,(byte)0x24,(byte)0xc1,(byte)0x00,(byte)0x20,(byte)0xc0,(byte)0x00,(byte)0xcb,
      (byte)0x1e,(byte)0x5a,(byte)0x91,(byte)0xb7,(byte)0xaa,(byte)0x51,(byte)0x64,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,(byte)0xae,(byte)0x42,(byte)0x60,
      (byte)0x82
    };

    int         row;
    Composite   composite;
    Label       label;
    Button      button;
    TableColumn tableColumn;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final FileComparator  fileComparator   = new FileComparator(FileComparator.SORTMODE_NAME);;
      final Updater         updater          = new Updater(fileComparator,listDirectory,(type != FileDialogTypes.DIRECTORY));
      final ArrayList<File> shortcutFileList = new ArrayList<File>();
      final HashSet<File> shortcutSet        = new HashSet<File>();

      // load images
      final Image IMAGE_FOLDER_UP;
      try
      {
        ByteArrayInputStream inputStream = new ByteArrayInputStream(IMAGE_FOLDER_UP_DATA_ARRAY);
        IMAGE_FOLDER_UP = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();
      }
      catch (Exception exception)
      {
        throw new Error(exception);
      }

      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,0.0,0.0},1.0));

      final Text   widgetPath;
      final Button widgetFolderUp;
      final List   widgetShortcutList;
      final Table  widgetFileList;
      final Combo  widgetFilter;
      final Button widgetShowHidden;
      final Text   widgetName;
      final Button widgetDone;
      final File   dragFile[] = new File[1];
      DragSource   dragSource;
      DropTarget   dropTarget;

      // path
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        label = new Label(composite,SWT.NONE);
        label.setText(Dialogs.tr("Path")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetPath = new Text(composite,SWT.NONE);
        Widgets.layout(widgetPath,0,1,TableLayoutData.WE);

        widgetFolderUp = new Button(composite,SWT.PUSH);
        widgetFolderUp.setImage(IMAGE_FOLDER_UP);
        Widgets.layout(widgetFolderUp,0,2,TableLayoutData.E);
      }

      // lists
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(1.0,new double[]{0.25,0.75},4));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));
      {
        widgetShortcutList = new List(composite,SWT.BORDER);
        widgetShortcutList.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

        widgetFileList = new Table(composite,SWT.BORDER);
        widgetFileList.setHeaderVisible(true);
        widgetFileList.setLinesVisible(true);
        widgetFileList.setLayout(new TableLayout(new double[]{1.0,0.0,0.0,0.0},1.0));
        widgetFileList.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));

        SelectionListener selectionListener = new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn tableColumn = (TableColumn)selectionEvent.widget;

            if      (tableColumn == widgetFileList.getColumn(0)) fileComparator.setSortMode(FileComparator.SORTMODE_NAME    );
            else if (tableColumn == widgetFileList.getColumn(1)) fileComparator.setSortMode(FileComparator.SORTMODE_TYPE    );
            else if (tableColumn == widgetFileList.getColumn(2)) fileComparator.setSortMode(FileComparator.SORTMODE_MODIFIED);
            else if (tableColumn == widgetFileList.getColumn(3)) fileComparator.setSortMode(FileComparator.SORTMODE_SIZE    );
            Widgets.sortTableColumn(widgetFileList,tableColumn,fileComparator);
          }
        };

        tableColumn = new TableColumn(widgetFileList,SWT.LEFT);
        tableColumn.setText("Name");
        tableColumn.setData(new TableLayoutData(0,0,TableLayoutData.WE,0,0,0,0,600,SWT.DEFAULT));
        tableColumn.setResizable(true);
        tableColumn.addSelectionListener(selectionListener);

        tableColumn = new TableColumn(widgetFileList,SWT.LEFT);
        tableColumn.setText("Type");
        tableColumn.setData(new TableLayoutData(0,1,TableLayoutData.NONE));
        tableColumn.setWidth(50);
        tableColumn.setResizable(false);
        tableColumn.addSelectionListener(selectionListener);

        tableColumn = new TableColumn(widgetFileList,SWT.LEFT);
        tableColumn.setText("Modified");
        tableColumn.setData(new TableLayoutData(0,2,TableLayoutData.NONE));
        tableColumn.setWidth(160);
        tableColumn.setResizable(false);
        tableColumn.addSelectionListener(selectionListener);

        tableColumn = new TableColumn(widgetFileList,SWT.RIGHT);
        tableColumn.setText("Size");
        tableColumn.setData(new TableLayoutData(0,3,TableLayoutData.NONE));
        tableColumn.setWidth(80);
        tableColumn.setResizable(false);
        tableColumn.addSelectionListener(selectionListener);
      }

      // filter, name
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0},4));
      composite.setLayoutData(new TableLayoutData(2,0,TableLayoutData.WE));
      {
        if (fileExtensions != null)
        {
          label = new Label(composite,SWT.NONE);
          label.setText(Dialogs.tr("Filter")+":");
          Widgets.layout(label,0,0,TableLayoutData.W);

          widgetFilter = new Combo(composite,SWT.NONE);
          widgetFilter.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE));
        }
        else
        {
          widgetFilter = null;
        }

        widgetShowHidden = new Button(composite,SWT.CHECK);
        widgetShowHidden.setText(Dialogs.tr("show hidden"));
        widgetShowHidden.setLayoutData(new TableLayoutData(0,2,TableLayoutData.E));

        if ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.SAVE))
        {
          label = new Label(composite,SWT.NONE);
          label.setText(Dialogs.tr("Name")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);

          widgetName = new Text(composite,SWT.NONE);
          Widgets.layout(widgetName,1,1,TableLayoutData.WE,0,2);
        }
        else
        {
          widgetName = null;
        }
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(4,0,TableLayoutData.WE,0,0,4));
      {
        widgetDone = new Button(composite,SWT.CENTER);
        switch (type)
        {
          case OPEN:
          default:
            widgetDone.setText(Dialogs.tr("Open"));
            break;
          case SAVE:
            widgetDone.setText(Dialogs.tr("Save"));
            break;
          case DIRECTORY:
            widgetDone.setText(Dialogs.tr("Select"));
            break;
        }
        widgetDone.setEnabled(false);
        widgetDone.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));

        button = new Button(composite,SWT.CENTER);
        button.setText(Dialogs.tr("Cancel"));
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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

      // install listeners
      widgetPath.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetFolderUp.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          File file = (File)widgetPath.getData();

          File parentFile = file.getParentFile();
          if (parentFile != null)
          {
            widgetPath.setData(parentFile);
            widgetPath.setText(parentFile.getAbsolutePath());
            updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
            widgetDone.setEnabled((parentFile != null));
          }
        }
      });
      widgetShortcutList.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          int index = widgetShortcutList.getSelectionIndex();
          if (index >= 0)
          {
            File file = shortcutFileList.get(index);
            if      (listDirectory.isDirectory(file))
            {
              widgetPath.setData(file);
              widgetPath.setText(file.getAbsolutePath());
              updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
              widgetDone.setEnabled((file != null));
            }
            else if (listDirectory.exists(file))
            {
              error(dialog,Dialogs.tr("''{0}'' is not a directory!",file.getAbsolutePath()));
            }
            else
            {
              error(dialog,Dialogs.tr("''{0}'' does not exists",file.getAbsolutePath()));
            }
          }
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetFileList.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          int index = widgetFileList.getSelectionIndex();
          if (index >= 0)
          {
            TableItem tableItem = widgetFileList.getItem(index);
            File      file      = (File)tableItem.getData();

            if (file.isDirectory())
            {
              File newPath = new File((File)widgetPath.getData(),file.getName());
              widgetPath.setData(newPath);
              widgetPath.setText(newPath.getAbsolutePath());
              updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
              widgetDone.setEnabled((newPath != null));
            }
          }
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          int index = widgetFileList.getSelectionIndex();
          if (index >= 0)
          {
            TableItem tableItem = widgetFileList.getItem(index);
            File      file      = (File)tableItem.getData();

            if (listDirectory.isFile(file))
            {
              if (widgetName != null) widgetName.setText(file.getName());
            }
          }
        }
      });
      widgetFileList.addMouseListener(new MouseListener()
      {
        public void mouseDoubleClick(MouseEvent mouseEvent)
        {
          TableItem[] tableItems = widgetFileList.getSelection();
          if (tableItems.length > 0)
          {
            File file = (File)tableItems[0].getData();
            if (file.isDirectory())
            {
              updater.updateFileList(widgetFileList,file.getAbsolutePath(),(widgetName != null) ? widgetName.getText() : null);
            }
            else
            {
              fileGeometry = dialog.getSize();
              close(dialog,
                    (widgetName != null)
                      ? new File(widgetPath.getText(),widgetName.getText()).getAbsolutePath()
                      : new File(widgetPath.getText()).getAbsolutePath()
                   );
            }
          }
        }
        public void mouseDown(MouseEvent mouseEvent)
        {
        }
        public void mouseUp(MouseEvent mouseEvent)
        {
        }
      });
      if (widgetFilter != null)
      {
        widgetFilter.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int index = widgetFilter.getSelectionIndex();
            if (index >= 0)
            {
              widgetFilter.setText(fileExtensions[index*2+1]);

              updater.setFileFilter(widgetFilter.getText());
              updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
            }
          }
        });
      }
      widgetShowHidden.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          updater.setShowHidden(widgetShowHidden.getSelection());
          updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);
        }
      });
      widgetDone.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          fileGeometry = dialog.getSize();
          File file = (File)widgetPath.getData();
          close(dialog,
                (widgetName != null)
                  ? new File(file,widgetName.getText()).getAbsolutePath()
                  : file.getAbsolutePath()
               );
        }
      });

      // drag+drop
      dragSource = new DragSource(widgetFileList,DND.DROP_MOVE);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);
          TableItem tableItem = widgetFileList.getItem(point);
          if ((tableItem != null) && listDirectory.isDirectory((File)tableItem.getData()))
          {
            dragFile[0] = (File)tableItem.getData();
          }
          else
          {
            dragSourceEvent.doit = false;
          }
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Point point         = new Point(dragSourceEvent.x,dragSourceEvent.y);
          TableItem tableItem = widgetFileList.getItem(point);
          if (tableItem != null)
          {
            dragSourceEvent.data = dragFile[0].getAbsolutePath();
          }
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
          dragFile[0] = null;
        }
      });
      dropTarget = new DropTarget(widgetShortcutList,DND.DROP_MOVE|DND.DROP_COPY);
      dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dropTarget.addDropListener(new DropTargetAdapter()
      {
        public void dragLeave(DropTargetEvent dropTargetEvent)
        {
        }
        public void dragOver(DropTargetEvent dropTargetEvent)
        {
        }
        public void drop(DropTargetEvent dropTargetEvent)
        {
          if (dropTargetEvent.data != null)
          {
            if (dropTargetEvent.data instanceof File)
            {
              shortcutSet.add((File)dropTargetEvent.data);
              listDirectory.setShortcuts(shortcutSet.toArray(new File[shortcutSet.size()]));

              updater.updateShortcutList(shortcutFileList,widgetShortcutList,shortcutSet);
            }
          }
          else
          {
            dropTargetEvent.detail = DND.DROP_NONE;
          }
        }
      });

      // show
      show(dialog,fileGeometry);

      // update shortcuts
      File shortcuts[] = listDirectory.getShortcuts();
      if (shortcuts != null)
      {
        for (File shortcut : shortcuts)
        {
          shortcutSet.add(shortcut);
        }
      }
      updater.updateShortcutList(shortcutFileList,widgetShortcutList,shortcutSet);

      // update path, name
      File file = new File(oldFileName);
      File parentFile = file.getParentFile();
      widgetPath.setData(parentFile);
      if (parentFile != null)
      {
        widgetPath.setText(file.getParentFile().getAbsolutePath());
      }
      if (widgetName != null)
      {
        widgetName.setText(file.getName());
      }
      widgetDone.setEnabled((parentFile != null));

      if ((fileExtensions != null) && (widgetFilter != null))
      {
        // update file extensions
        for (int i = 0; i < fileExtensions.length; i+= 2)
        {
          widgetFilter.add(fileExtensions[i+0]+" ("+fileExtensions[i+1]+")");
        }
        widgetFilter.setText(defaultFileExtension);
        updater.setFileFilter(defaultFileExtension);
       }

      // update file list
      updater.updateFileList(widgetFileList,widgetPath.getText(),(widgetName != null) ? widgetName.getText() : null);

      if (widgetName != null)
      {
        widgetName.setFocus();
        widgetName.setSelection(new Point(0,widgetName.getText().length()));
      }
      else
      {
        widgetPath.setFocus();
        widgetPath.setSelection(new Point(0,widgetPath.getText().length()));
      }
      return (String)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param fileName fileName or null
   * @return file name or null
   */
  public static String file(Shell               parentShell,
                            FileDialogTypes     type,
                            String              title,
                            String              oldFileName,
                            final ListDirectory listDirectory
                           )
  {
    return file(parentShell,type,title,oldFileName,(String[])null,(String)null,listDirectory);
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
  private static String file(Shell    parentShell,
                             int      type,
                             String   title,
                             String   oldFileName,
                             String[] fileExtensions,
                             String   defaultFileExtension
                            )
  {
    File oldFile = (oldFileName != null) ? new File(oldFileName) : null;

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
  public static String fileOpen(Shell    parentShell,
                                String   title,
                                String   fileName,
                                String[] fileExtensions,
                                String   defaultFileExtension
                               )
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
  public static String fileOpen(Shell    parentShell,
                                String   title,
                                String   fileName,
                                String[] fileExtensions
                               )
  {
    return fileOpen(parentShell,title,fileName,fileExtensions,null);
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @return file name or null
   */
  public static String fileOpen(Shell  parentShell,
                                String title,
                                String fileName
                               )
  {
    return fileOpen(parentShell,title,fileName,null);
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @return file name or null
   */
  public static String fileOpen(Shell  parentShell,
                                String title
                               )
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
  public static String fileSave(Shell    parentShell,
                                String   title,
                                String   fileName,
                                String[] fileExtensions,
                                String   defaultFileExtension
                               )
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
  public static String fileSave(Shell    parentShell,
                                String   title,
                                String   fileName,
                                String[] fileExtensions
                               )
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
  public static String fileSave(Shell  parentShell,
                                String title,
                                String fileName,
                                String defaultFileExtension
                               )
  {
    return fileSave(parentShell,title,fileName,null,defaultFileExtension);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @return file name or null
   */
  public static String fileSave(Shell  parentShell,
                                String title,
                                String fileName
                               )
  {
    return fileSave(parentShell,title,fileName,(String)null);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @return file name or null
   */
  public static String fileSave(Shell  parentShell,
                                String title
                               )
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
  public static String directory(Shell  parentShell,
                                 String title,
                                 String text,
                                 String pathName
                                )
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
  public static String directory(Shell  parentShell,
                                 String title,
                                 String pathName
                                )
  {
    return directory(parentShell,title,null,pathName);
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @return directory name or null
   */
  public static String directory(Shell  parentShell,
                                 String title
                                )
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
  public static String path(Shell  parentShell,
                            String title,
                            String text,
                            String value,
                            String okText,
                            String cancelText,
                            String toolTipText
                           )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"directory.png");

    int       row;
    Composite composite;
    Label     label;
    Button    button;

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
        button.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String path = directory(dialog,Dialogs.tr("Select path"));
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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E));
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
  public static String path(Shell  parentShell,
                            String title,
                            String text,
                            String value,
                            String okText,
                            String cancelText
                           )
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
  public static String path(Shell  parentShell,
                            String title,
                            String text,
                            String value
                           )
  {
    return path(parentShell,title,text,value,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** simple path dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @return path or null on cancel
   */
  public static String path(Shell  parentShell,
                            String title,
                            String text
                           )
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
  public static String string(Shell  parentShell,
                              String title,
                              String text,
                              String value,
                              String okText,
                              String cancelText,
                              String toolTipText
                             )
  {
    int       row;
    Composite composite;
    Label     label;
    Button    button;

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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
  public static String string(Shell  parentShell,
                              String title,
                              String text,
                              String value,
                              String okText,
                              String cancelText
                             )
  {
    return string(parentShell,title,text,value,okText,cancelText,null);
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @return string or null on cancel
   */
  public static String string(Shell  parentShell,
                              String title,
                              String text,
                              String value,
                              String okText
                             )
  {
    return string(parentShell,title,text,value,okText,Dialogs.tr("Cancel"));
  }


  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @return string or null on cancel
   */
  public static String string(Shell  parentShell,
                              String title,
                              String text,
                              String value
                             )
  {
    return string(parentShell,title,text,value,Dialogs.tr("Save"));
  }

  /** simple string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @return string or null on cancel
   */
  public static String string(Shell  parentShell,
                              String title,
                              String text
                             )
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
  public static Integer integer(Shell  parentShell,
                                String title,
                                String text,
                                int    value,
                                int    minValue,
                                int    maxValue,
                                String okText,
                                String cancelText,
                                String toolTipText
                               )
  {
    int       row;
    Composite composite;
    Label     label;
    Button    button;

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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
  public static Integer integer(Shell  parentShell,
                                String title,
                                String text,
                                int    value,
                                int    minValue,
                                int    maxValue,
                                String okText,
                                String cancelText
                               )
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
  public static Integer integer(Shell  parentShell,
                                String title,
                                String text,
                                int    value,
                                int    minValue,
                                int    maxValue
                               )
  {
    return integer(parentShell,title,text,value,minValue,maxValue,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** slider dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit
   * @param minValue,maxValue min./max. value
   * @param increment increment value
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return value or null on cancel
   */
  public static Integer slider(Shell     parentShell,
                               String    title,
                               String    text,
                               final int value,
                               int       minValue,
                               int       maxValue,
                               final int increment,
                               String    okText,
                               String    cancelText,
                               String    toolTipText
                              )
  {
    int       row;
    Composite composite,subComposite;
    Label     label;
    Button    button;

    if (increment < 1) throw new IllegalArgumentException();

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,100,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // slider
      final Combo  widgetValue;
      final Slider widgetSlider;
      final Button widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        label = new Label(composite,SWT.LEFT);
        label.setText(text);
        label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW));

        subComposite = new Composite(composite,SWT.NONE);
        subComposite.setLayout(new TableLayout(1.0,1.0,4));
        subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE));
        {
          widgetValue = new Combo(subComposite,SWT.RIGHT|SWT.READ_ONLY);
          widgetValue.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));

          widgetSlider = new Slider(subComposite,SWT.HORIZONTAL);
          widgetSlider.setMinimum(minValue);
          widgetSlider.setMaximum(maxValue);
          widgetSlider.setIncrement(increment);
          widgetSlider.setSelection(value);
          widgetSlider.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));//,0,0,0,0,100,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
          if (toolTipText != null) widgetSlider.setToolTipText(toolTipText);
        }
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int newValue = (widgetSlider.getSelection() / increment) * increment;
            close(dialog,new Integer(newValue));
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,new Integer(value));
          }
        });
      }

      // install handlers
      widgetValue.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetOkButton.setFocus();
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetSlider.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          int newValue = (widgetSlider.getSelection() / increment) * increment;
          widgetValue.select(newValue / increment);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      // set values
      int i = 0;
      for (int n = minValue; n <= maxValue; n += increment)
      {
        widgetValue.add(String.format("%d",n));
        if (n == value) widgetValue.select(i);
        i++;
      }

      widgetSlider.setFocus();
      return (Integer)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list values getter
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String list(Shell        parentShell,
                            String       title,
                            String       text,
                            ListRunnable listRunnable,
                            String       okText,
                            String       cancelText,
                            String       toolTipText
                           )
  {
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final String[] result = new String[1];

      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      double[] rowWeights = new double[2];
      int row = 0;
      if (text != null)
      {
        rowWeights[row] = 0.0; row++;
      }
      rowWeights[row] = 1.0; row++;

      // list
      final List   widgetList;
      final Button widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(rowWeights,1.0,4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        row = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W));
          row++;
        }

        widgetList = new List(composite,SWT.BORDER|SWT.V_SCROLL);
        if (toolTipText != null) widgetList.setToolTipText(toolTipText);
        widgetList.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE,0,0,0,0,300,200,SWT.DEFAULT,SWT.DEFAULT));
        row++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String selection[] = widgetList.getSelection();
            close(dialog,(selection.length > 0) ? selection[0] : (String)null);
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,100,SWT.DEFAULT));
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
      widgetList.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          List widget = (List)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetList.addMouseListener(new MouseListener()
      {
        public void mouseDoubleClick(MouseEvent mouseEvent)
        {
          List widget = (List)mouseEvent.widget;

          String selection[] = widgetList.getSelection();
          close(dialog,(selection.length > 0) ? selection[0] : (String)null);
        }
        public void mouseDown(MouseEvent mouseEvent)
        {
        }
        public void mouseUp(MouseEvent mouseEvent)
        {
        }
      });

      // show
      show(dialog);

      // fill-in value
      String values[] = listRunnable.getValues();
      if (values == null)
      {
        return null;
      }
      String value    = listRunnable.getSelection();
      if (value == null)
      {
        return null;
      }
      int index = -1;
      for (int i = 0; i < values.length; i++)
      {
        widgetList.add(values[i]);
        if (value.equals(values[i])) index = i;
      }
      if (index >= 0) widgetList.setSelection(index);

      widgetList.setFocus();
      return (String)run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list values getter
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String list(Shell        parentShell,
                            String       title,
                            String       text,
                            ListRunnable listRunnable,
                            String       okText,
                            String       cancelText
                           )
  {
    return list(parentShell,title,text,listRunnable,okText,cancelText,(String)null);
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list values getter
   * @param okText OK button text
   * @return string or null on cancel
   */
  public static String list(Shell        parentShell,
                            String       title,
                            String       text,
                            ListRunnable listRunnable,
                            String       okText
                           )
  {
    return list(parentShell,title,text,listRunnable,okText,Dialogs.tr("Cancel"));
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list values getter
   * @return string or null on cancel
   */
  public static String list(Shell        parentShell,
                            String       title,
                            String       text,
                            ListRunnable listRunnable
                           )
  {
    return list(parentShell,title,text,listRunnable,Dialogs.tr("Select"));
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param values value to list
   * @param value current value
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String list(Shell        parentShell,
                            String       title,
                            String       text,
                            final String values[],
                            final String value,
                            String       okText,
                            String       cancelText,
                            String       toolTipText
                           )
  {
    return list(parentShell,
                title,
                text,
                new ListRunnable()
                {
                  public String[] getValues()
                  {
                    return values;
                  }
                  public String getSelection()
                  {
                    return value;
                  }
                },
                okText,
                cancelText,
                toolTipText
               );
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param values value to list
   * @param value current value
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String list(Shell  parentShell,
                            String title,
                            String text,
                            String values[],
                            String value,
                            String okText,
                            String cancelText
                           )
  {
    return list(parentShell,title,text,values,value,okText,cancelText,(String)null);
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param values value to list
   * @param value current value
   * @param okText OK button text
   * @return string or null on cancel
   */
  public static String list(Shell  parentShell,
                            String title,
                            String text,
                            String values[],
                            String value,
                            String okText
                           )
  {
    return list(parentShell,title,text,values,value,okText,Dialogs.tr("Cancel"));
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param values value to list
   * @param value current value
   * @return string or null on cancel
   */
  public static String list(Shell  parentShell,
                            String title,
                            String text,
                            String values[],
                            String value
                           )
  {
    return list(parentShell,title,text,values,value,Dialogs.tr("Select"));
  }

  /** simple list select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param values value to list
   * @return string or null on cancel
   */
  public static String list(Shell  parentShell,
                            String title,
                            String text,
                            String values[]
                           )
  {
    return list(parentShell,title,text,values,(String)null);
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
    return openSimpleBusy(parentShell,Dialogs.tr("Busy"),message,abortButton);
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
    return openBusy(parentShell,Dialogs.tr("Busy"),message);
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
    return openSimpleProgress(parentShell,Dialogs.tr("Progress"),message);
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
