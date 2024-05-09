/***********************************************************************\
*
* Contents: dialog functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.ParameterizedType;

import java.text.MessageFormat;
import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Arrays;
import java.util.BitSet;
import java.util.Calendar;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import java.util.Collection;

import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.StyledText;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSource;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.DropTarget;
import org.eclipse.swt.dnd.DropTargetAdapter;
import org.eclipse.swt.dnd.DropTargetEvent;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
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
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.DateTime;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
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
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
    display.syncExec(new Runnable()
    {
      public void run()
      {
        if (!dialog.isDisposed())
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
      }
    });
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
    display.syncExec(new Runnable()
    {
      public void run()
      {
        if (!dialog.isDisposed())
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
      }
    });
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
  public abstract void done(Object result);
}

/** list values runnable
 */
abstract class ListRunnable
{
  /** get list values
   * @param list values
   */
  public abstract Collection<String> getValues();

  /** get selected value
   * @param selected list value
   */
  public abstract String getSelection();
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
abstract class ListDirectory<T extends File> implements Comparator<T>
{
  private HashMap<String,T> shortcutMap = new HashMap<String,T>();

  /** create list directory
   */
  public ListDirectory()
  {
    for (T root : getRoots())
    {
      shortcutMap.put(root.getName(),root);
    }
  }

  /** get new file instance
   * @param path path (can be null)
   * @param name name
   * @return file instance
   */
  public abstract T newFileInstance(String path, String name);

  /** get new file instance
   * @param name name
   * @return file instance
   */
  public T newFileInstance(String name)
  {
    return newFileInstance((String)null,name);
  }

  /** get root file instances
   * @return root file instances
   */
  public ArrayList<T> getRoots()
  {
    ArrayList<T> roots = new ArrayList<T>();
    roots.add(newFileInstance("/"));

    return roots;
  }

  /** get default root file instance
   * @return default root file instance
   */
  public T getDefaultRoot()
  {
    return getRoots().get(0);
  }

  /** get parent file instance
   * @param file file
   * @return parent file instance or null
   */
  public abstract T getParentFile(T file);

  /** get absolute path
   * @param file file
   * @return absolute path
   */
  public abstract String getAbsolutePath(T file);

  /** compare files
   * @return -1/0/1 if file0 </=/> file1
   */
  public int compare(T file0, T file1)
  {
    return getAbsolutePath(file0).compareTo(getAbsolutePath(file1));
  }

  /** get shortcut files
   * @param shortcut file collection
   */
  public void getShortcuts(java.util.List<T> shortcutList)
  {
    shortcutList.clear();
    for (T root : getRoots())
    {
      shortcutMap.put(root.getName(),root);
    }
    for (T shortcut : shortcutMap.values())
    {
      shortcutList.add(shortcut);
    }
    Collections.sort(shortcutList,this);
  }

  /** add shortcut file
   * @param shortcut shortcut file
   */
  public void addShortcut(T shortcut)
  {
    shortcutMap.put(getAbsolutePath(shortcut),shortcut);
  }

  /** add shortcut file
   * @param name shortcut name
   */
  public void addShortcut(String name)
  {
    addShortcut(newFileInstance(name));
  }

  /** remove shortcut file
   * @param shortcut shortcut file
   */
  public void removeShortcut(T shortcut)
  {
    shortcutMap.remove(getAbsolutePath(shortcut));
  }

  /** open list files in directory
   * @param path path
   * @return true iff open
   */
  public abstract boolean open(T path);

  /** close list files in directory
   */
  public abstract void close();

  /** get next entry in directory
   * @return entry
   */
  public abstract T getNext();

  /** check if file is root entry
   * @param file file to check
   * @return true iff is root entry
   */
  public abstract boolean isRoot(T file);

  /** check if directory
   * @param file file to check
   * @return true if file is directory
   */
  public abstract boolean isDirectory(T file);

  /** check if file
   * @param file file to check
   * @return true if file is file
   */
  public abstract boolean isFile(T file);

  /** check if hidden
   * @param file file to check
   * @return true if file is hidden
   */
  public abstract boolean isHidden(T file);

  /** check if exists
   * @param file file to check
   * @return true if file exists
   */
  public abstract boolean exists(T file);

  /** make directory
   * @param directory directory to create
   */
  public abstract void mkdir(T directory)
    throws IOException;

  /** delete file or directory
   * @param file file or directory to delete
   */
  public abstract void delete(T file)
    throws IOException;
}

abstract class ListDirectoryFilter<T extends File>
{
  /** check if accepted
   * @param file file to check
   * @return true iff accepted
   */
  public abstract boolean isAccepted(T file);
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
    DIRECTORY,
    ENTRY
  };

  public final static int FILE_NONE        = 0;
  public final static int FILE_SHOW_HIDDEN = 1 << 0;

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
    if (!dialog.isDisposed())
    {
      Display display = dialog.getDisplay();

      dialog.setData(returnValue);
      dialog.close();

      // Note: sometimes it seems the close() does not generate a wake-up of the main event loop?
      if (!display.isDisposed())
      {
        display.wake();
      }
    }
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
   * @return value
   */
  public static <T> T run(final Shell dialog, final T escapeKeyReturnValue, final DialogRunnable dialogRunnable)
  {
    final T[] result = (T[])new Object[1];

    if (!dialog.isDisposed())
    {
      final Display display = dialog.getDisplay();

      // add escape key handler
      dialog.addTraverseListener(new TraverseListener()
      {
        @Override
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
        @Override
        public void handleEvent(Event event)
        {
          // close the dialog
          dialog.dispose();
        }
      });

      // add result handler
      dialog.addDisposeListener(new DisposeListener()
      {
        @Override
        public void widgetDisposed(DisposeEvent disposeEvent)
        {
          // get result
          result[0] = (T)disposeEvent.widget.getData();

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
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog);
            }
            @Override
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
  public static void error(Shell                     parentShell,
                           final BooleanFieldUpdater showAgainFieldFlag,
                           String[]                  extendedMessage,
                           String                    message
                          )
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
            @Override
            public void mouseDoubleClick(MouseEvent mouseEvent)
            {
              Text widget = (Text)mouseEvent.widget;

              widget.setSelection(0,widget.getText().length());
            }
            @Override
            public void mouseDown(MouseEvent mouseEvent)
            {
            }
            @Override
            public void mouseUp(MouseEvent mouseEvent)
            {
            }
          });
          row++;

          if (extendedMessage != null)
          {
            // get extened message (limit to 80 characters per line)
            StringBuilder buffer = new StringBuilder();
            for (String string : extendedMessage)
            {
              if (buffer.length() > 0) buffer.append(Text.DELIMITER);
              if (string.length() > 80)
              {
                string = string.substring(0,80)+"\u2026";
              }
              buffer.append(string);
            }

            label = new Label(composite,SWT.LEFT);
            label.setText(Dialogs.tr("Extended error")+":");
            label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
            row++;

            text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.READ_ONLY);
            text.setText(buffer.toString());
            text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,120));
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
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog);
            }
            @Override
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
  public static void error(Shell                  parentShell,
                           BooleanFieldUpdater    showAgainFieldFlag,
                           java.util.List<String> extendedMessage,
                           String                 message
                          )
  {
    error(parentShell,showAgainFieldFlag,extendedMessage.toArray(new String[extendedMessage.size()]),message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message error message
   */
  public static void error(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              message
                          )
  {
    error(parentShell,showAgainFieldFlag,(String[])null,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   * @param extendedMessage extended message
   */
  public static void error(Shell  parentShell,
                           String extendedMessage[],
                           String message
                          )
  {
    error(parentShell,(BooleanFieldUpdater)null,extendedMessage,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   * @param extendedMessage extended message
   */
  public static void error(Shell                  parentShell,
                           java.util.List<String> extendedMessage,
                           String                 message
                          )
  {
    error(parentShell,extendedMessage.toArray(new String[extendedMessage.size()]),message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   */
  public static void error(Shell  parentShell,
                           String message
                          )
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
  public static void error(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String[]            extendedMessage,
                           String              format,
                           Object...           arguments
                          )
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
  public static void error(Shell                  parentShell,
                           BooleanFieldUpdater    showAgainFieldFlag,
                           java.util.List<String> extendedMessage,
                           String                 format,
                           Object...              arguments
                          )
  {
    error(parentShell,showAgainFieldFlag,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell     parentShell,
                           String[]  extendedMessage,
                           String    format,
                           Object... arguments
                          )
  {
    error(parentShell,(BooleanFieldUpdater)null,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell                  parentShell,
                           java.util.List<String> extendedMessage,
                           String                 format,
                           Object...              arguments
                          )
  {
    error(parentShell,(BooleanFieldUpdater)null,extendedMessage,String.format(format,arguments));
  }

  /** error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              format,
                           Object...           arguments
                          )
  {
    error(parentShell,showAgainFieldFlag,(String[])null,format,arguments);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  public static void error(Shell     parentShell,
                           String    format,
                           Object... arguments
                          )
  {
    error(parentShell,(BooleanFieldUpdater)null,format,arguments);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message error message
   */
  static void warning(Shell                     parentShell,
                      final BooleanFieldUpdater showAgainFieldFlag,
                      String[]                  extendedMessage,
                      String                    message
                     )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"warning.png");

    Composite composite;
    Label     label;
    Button    button;
    Text      text;

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final Shell dialog = openModal(parentShell,Dialogs.tr("Warning"),200,70);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        // message
        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          int row = 0;

          label = new Label(composite,SWT.LEFT);
          label.setImage(IMAGE);
          label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
          row++;

          if (extendedMessage != null)
          {
            // get extened message (limit to 80 characters per line)
            StringBuilder buffer = new StringBuilder();
            for (String string : extendedMessage)
            {
              if (buffer.length() > 0) buffer.append(Text.DELIMITER);
              if (string.length() > 80)
              {
                string = string.substring(0,80)+"\u2026";
              }
              buffer.append(string);
            }

            label = new Label(composite,SWT.LEFT);
            label.setText(Dialogs.tr("Extended message")+":");
            label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
            row++;

            text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.READ_ONLY);
            text.setText(buffer.toString());
            text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,120));
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
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,120,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              close(dialog);
            }
            @Override
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
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message error message
   */
  static void warning(Shell                     parentShell,
                      final BooleanFieldUpdater showAgainFieldFlag,
                      java.util.List<String>    extendedMessage,
                      String                    message
                     )
  {
    warning(parentShell,showAgainFieldFlag,extendedMessage.toArray(new String[extendedMessage.size()]),message);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param message error message
   */
  static void warning(Shell  parentShell,
                      String extendedMessage[],
                      String message
                     )
  {
    warning(parentShell,(BooleanFieldUpdater)null,extendedMessage,message);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void warning(Shell  parentShell,
                      String message
                     )
  {
    warning(parentShell,(String[])null,message);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell               parentShell,
                      BooleanFieldUpdater showAgainFieldFlag,
                      String[]            extendedMessage,
                      String              format,
                      Object...           arguments
                     )
  {
    warning(parentShell,showAgainFieldFlag,extendedMessage,String.format(format,arguments));
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell               parentShell,
                      BooleanFieldUpdater showAgainFieldFlag,
                      String              format,
                      Object...           arguments
                     )
  {
    warning(parentShell,showAgainFieldFlag,(String[])null,format,arguments);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell     parentShell,
                      String[]  extendedMessage,
                      String    format,
                      Object... arguments
                     )
  {
    warning(parentShell,(BooleanFieldUpdater)null,extendedMessage,format,arguments);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell     parentShell,
                      String    format,
                      Object... arguments
                     )
  {
    warning(parentShell,(BooleanFieldUpdater)null,format,arguments);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param image image to show
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value if no show-again field updater or show-again checkbox is true,
             defaultValue otherwise
   */
  public static boolean confirm(Shell                     parentShell,
                                String                    title,
                                final BooleanFieldUpdater showAgainFieldFlag,
                                Image                     image,
                                String[]                  extendedMessage,
                                String                    message,
                                String                    yesText,
                                String                    noText,
                                boolean                   defaultValue
                               )
  {
    Composite composite;
    Label     label;
    Button    button;
    Text      text;

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
          int row = 0;

          label = new Label(composite,SWT.LEFT);
          label.setImage(image);
          label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W,0,0,10));

          label = new Label(composite,SWT.LEFT|SWT.WRAP);
          label.setText(message);
          label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
          row++;

          if (extendedMessage != null)
          {
            // get extened message (limit to 80 characters per line)
            StringBuilder buffer = new StringBuilder();
            for (String string : extendedMessage)
            {
              if (buffer.length() > 0) buffer.append(Text.DELIMITER);
              if (string.length() > 80)
              {
                string = string.substring(0,80)+"\u2026";
              }
              buffer.append(string);
            }

            label = new Label(composite,SWT.LEFT);
            label.setText(Dialogs.tr("Extended error")+":");
            label.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,4));
            row++;

            text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.READ_ONLY);
            text.setText(buffer.toString());
            text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,120));
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
          button.setText(yesText);
          if (defaultValue == true) button.setFocus();
          button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog,true);
            }
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = new Button(composite,SWT.CENTER);
          button.setText(noText);
          if (defaultValue == false) button.setFocus();
          button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              close(dialog,false);
            }
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        Boolean result = (Boolean)run(dialog,
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
        return (result != null) ? result : defaultValue;
      }
      else
      {
        return defaultValue;
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
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return value
   */
  public static boolean confirm(Shell    parentShell,
                                String   title,
                                Image    image,
                                String[] extendedMessage,
                                String   message,
                                String   yesText,
                                String   noText,
                                boolean  defaultValue
                               )
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,image,extendedMessage,message,yesText,noText,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param image image to show
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell               parentShell,
                                String              title,
                                BooleanFieldUpdater showAgainFieldFlag,
                                Image               image,
                                String[]            extendedMessage,
                                String              message,
                                String              yesText,
                                String              noText
                               )
  {
    return confirm(parentShell,title,showAgainFieldFlag,image,extendedMessage,message,yesText,noText,false);
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
  public static boolean confirm(Shell  parentShell,
                                String title,
                                Image  image,
                                String message,
                                String yesText,
                                String noText
                               )
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,image,(String[])null,message,yesText,noText);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image to show
   * @param message confirmation message
   * @param yesText yes-text
   * @return value
   */
  public static boolean confirm(Shell  parentShell,
                                String title,
                                Image  image,
                                String message,
                                String yesText
                               )
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,image,(String[])null,message,yesText,Dialogs.tr("Cancel"));
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
  public static boolean confirm(Shell               parentShell,
                                String              title,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message,
                                String              yesText,
                                String              noText,
                                boolean             defaultValue
                               )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return confirm(parentShell,title,showAgainFieldFlag,IMAGE,(String[])null,message,yesText,noText,defaultValue);
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
  public static boolean confirm(Shell   parentShell,
                                String  title,
                                String  message,
                                String  yesText,
                                String  noText,
                                boolean defaultValue
                               )
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
  public static boolean confirm(Shell               parentShell,
                                String              title,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message,
                                String              yesText,
                                String              noText
                               )
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,yesText,noText,false);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirm(Shell  parentShell,
                                String title,
                                String message,
                                String yesText,
                                String noText
                               )
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
  public static boolean confirm(Shell  parentShell,
                                String title,
                                String message,
                                String yesText
                               )
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
  public static boolean confirm(Shell               parentShell,
                                String              title,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message,
                                boolean             defaultValue
                               )
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,Dialogs.tr("Yes"),Dialogs.tr("No"),defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell   parentShell,
                                String  title,
                                String  message,
                                boolean defaultValue
                               )
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
  public static boolean confirm(Shell               parentShell,
                                String              title,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message
                               )
  {
    return confirm(parentShell,title,showAgainFieldFlag,message,Dialogs.tr("Yes"),Dialogs.tr("No"),false);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell  parentShell,
                                String title,
                                String message
                               )
  {
    return confirm(parentShell,title,(BooleanFieldUpdater)null,message);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell               parentShell,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message,
                                boolean             defaultValue
                               )
  {
    return confirm(parentShell,Dialogs.tr("Confirmation"),showAgainFieldFlag,message,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell   parentShell,
                                String  message,
                                boolean defaultValue
                               )
  {
    return confirm(parentShell,(BooleanFieldUpdater)null,message,defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell               parentShell,
                                BooleanFieldUpdater showAgainFieldFlag,
                                String              message
                               )
  {
    return confirm(parentShell,showAgainFieldFlag,message,false);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  public static boolean confirm(Shell  parentShell,
                                String message
                               )
  {
    return confirm(parentShell,(BooleanFieldUpdater)null,message);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell               parentShell,
                                     String              title,
                                     BooleanFieldUpdater showAgainFieldFlag,
                                     String[]            extendedMessage,
                                     String              message,
                                     String              yesText,
                                     String              noText
                                    )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"error.png");

    return confirm(parentShell,title,showAgainFieldFlag,IMAGE,extendedMessage,message,yesText,noText);
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
  public static boolean confirmError(Shell               parentShell,
                                     String              title,
                                     BooleanFieldUpdater showAgainFieldFlag,
                                     String              message,
                                     String              yesText,
                                     String              noText
                                    )
  {
    return confirmError(parentShell,title,showAgainFieldFlag,(String[])null,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell               parentShell,
                                     BooleanFieldUpdater showAgainFieldFlag,
                                     String[]            extendedMessage,
                                     String              message,
                                     String              yesText,
                                     String              noText
                                    )
  {
    return confirmError(parentShell,Dialogs.tr("Confirmation"),showAgainFieldFlag,extendedMessage,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell               parentShell,
                                     BooleanFieldUpdater showAgainFieldFlag,
                                     String              message,
                                     String              yesText,
                                     String              noText
                                    )
  {
    return confirmError(parentShell,showAgainFieldFlag,(String[])null,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell    parentShell,
                                     String   title,
                                     String[] extendedMessage,
                                     String   message,
                                     String   yesText,
                                     String   noText
                                    )
  {
    return confirmError(parentShell,title,(BooleanFieldUpdater)null,extendedMessage,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell  parentShell,
                                     String title,
                                     String message,
                                     String yesText,
                                     String noText
                                    )
  {
    return confirmError(parentShell,title,(String[])null,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param extendedMessage extended message
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell    parentShell,
                                     String[] extendedMessage,
                                     String   message,
                                     String   yesText,
                                     String   noText
                                    )
  {
    return confirmError(parentShell,Dialogs.tr("Confirmation"),(BooleanFieldUpdater)null,extendedMessage,message,yesText,noText);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  public static boolean confirmError(Shell  parentShell,
                                     String message,
                                     String yesText,
                                     String noText
                                    )
  {
    return confirmError(parentShell,(String[])null,message,yesText,noText);
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param helpText help text or null
   * @param okText ok-text
   * @param cancelText cancel-text
   * @param defaultValue default value (0..n-1)
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, final Object[] values, String helpText, String okText, String cancelText, T defaultValue)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    Composite composite,subComposite;
    Label     label;
    Combo     combo;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Object[] result = new Object[]{defaultValue};

      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        // image
        label = new Label(composite,SWT.LEFT);
        label.setImage(IMAGE);
        label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,10));

        subComposite = new Composite(composite,SWT.NONE);
        subComposite.setLayout(new TableLayout(0.0,(name != null) ? new double[]{0.0,1.0} : new double[]{1.0}));
        subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE,0,0,4));
        {
          int column = 0;

          if (name != null)
          {
            // name
            label = new Label(subComposite,SWT.LEFT|SWT.WRAP);
            label.setText(name);
            label.setLayoutData(new TableLayoutData(0,column,TableLayoutData.W,0,0,4)); column++;
          }

          combo = new Combo(subComposite,SWT.READ_ONLY);
          int selectedIndex = 0;
          for (int i = 0; i < values.length/2; i++)
          {
            combo.add((String)values[i*2+0]);
            if (values[i*2+1].equals(defaultValue)) selectedIndex = i;
          }
          combo.select(selectedIndex);
          combo.setLayoutData(new TableLayoutData(0,column,TableLayoutData.WE,0,0,4)); column++;
          combo.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo widget = (Combo)selectionEvent.widget;

              result[0] = values[widget.getSelectionIndex()*2+1];
            }
          });
          if (helpText != null)
          {
            combo.setToolTipText(helpText);
          }
        }
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        button = new Button(composite,SWT.CENTER);
        button.setText(okText);
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,true);
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,60,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,false);
          }
        });
      }

      if ((Boolean)run(dialog,false))
      {
        return (T)result[0];
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

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param helpText help text or null
   * @param okText ok-text
   * @param cancelText cancel-text
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, final Object[] values, String helpText, String okText, String cancelText)
  {
    return value(parentShell,title,name,values,helpText,okText,cancelText,(T)null);
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param helpText help text or null
   * @param okText ok-text
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, final Object[] values, String helpText, String okText)
  {
    return value(parentShell,title,name,values,helpText,okText,"Cancel");
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param helpText help text or null
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, final Object[] values, String helpText)
  {
    return value(parentShell,title,name,values,helpText,"OK");
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, Object[] values)
  {
    return value(parentShell,title,name,values,(String)null);
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param okText ok-text
   * @param defaultValue default value (0..n-1)
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, Object[] values, String okText, T defaultValue)
  {
    return value(parentShell,title,name,values,(String)null,okText,"Cancel",defaultValue);
  }

  /** select value
   * @param parentShell parent shell
   * @param title title string
   * @param name name of value
   * @param values array with [text, value]
   * @param defaultValue default value (0..n-1)
   * @return selected value or null
   */
  public static <T> T value(Shell parentShell, String title, String name, Object[] values, T defaultValue)
  {
    return value(parentShell,title,name,values,(String)null,"OK","Cancel",defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param image image
   * @param extendedMessageTitle extended message title or null
   * @param extendedMessage extended message or null
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param okText ok-text
   * @param cancelText cancel-text
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1) or -1
   */
  public static int select(Shell                     parentShell,
                           final BooleanFieldUpdater showAgainFieldFlag,
                           String                    title,
                           Image                     image,
                           String                    extendedMessageTitle,
                           String[]                  extendedMessage,
                           String                    message,
                           String[]                  texts,
                           String[]                  helpTexts,
                           boolean[]                 enabled,
                           String                    okText,
                           String                    cancelText,
                           int                       defaultValue
                          )
  {
    Composite composite,subComposite;
    Label     label;
    Text      text;
    Button    button;

    assert((helpTexts == null) || (helpTexts.length == texts.length));
    assert((enabled == null) || (enabled.length == texts.length));

    if ((showAgainFieldFlag == null) || showAgainFieldFlag.get())
    {
      if (!parentShell.isDisposed())
      {
        final int[] value = new int[1];

        final Shell dialog = openModal(parentShell,title);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          // image
          label = new Label(composite,SWT.LEFT);
          label.setImage(image);
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
              label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE,0,0,4));
              row++;
            }

            if (extendedMessage != null)
            {
              // get extened message (limit to 80 characters per line)
              StringBuilder buffer = new StringBuilder();
              for (String string : extendedMessage)
              {
                if (buffer.length() > 0) buffer.append(Text.DELIMITER);
                if (string.length() > 80)
                {
                  string = string.substring(0,80)+"\u2026";
                }
                buffer.append(string);
              }

              if (extendedMessageTitle != null)
              {
                // extended message title
                label = new Label(subComposite,SWT.LEFT|SWT.WRAP);
                label.setText(extendedMessageTitle);
                label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE,0,0,4));
                row++;
              }

              // extended message
              text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.READ_ONLY);
              text.setText(buffer.toString());
              text.setLayoutData(new TableLayoutData(row,1,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,120));
              row++;
            }

            // buttons
            if ((okText != null) || (cancelText != null))
            {
              Button selectedButton = null;
              for (int i = 0; i < texts.length; i++)
              {
                if (texts[i] != null)
                {
                  button = new Button(subComposite,SWT.RADIO);
                  button.setEnabled((enabled == null) || enabled[i]);
                  button.setText(texts[i]);
                  button.setData(i);
                  if (   ((enabled == null) || enabled[i])
                      && ((i == defaultValue) || (selectedButton == null))
                     )
                  {
                    if (selectedButton != null) selectedButton.setSelection(false);

                    button.setSelection(true);
                    value[0] = i;
                    selectedButton = button;
                  }
                  button.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W)); row++;
                  button.addSelectionListener(new SelectionListener()
                  {
                    @Override
                    public void widgetDefaultSelected(SelectionEvent selectionEvent)
                    {
                    }
                    @Override
                    public void widgetSelected(SelectionEvent selectionEvent)
                    {
                      Button widget = (Button)selectionEvent.widget;

                      value[0] = (Integer)widget.getData();
                    }
                  });
                  if ((helpTexts != null) && (i < helpTexts.length) && (helpTexts[i] != null))
                  {
                    button.setToolTipText(helpTexts[i]);
                  }
                }
              }
            }
          }

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
          if ((okText != null) || (cancelText != null))
          {
            if (okText != null)
            {
              button = new Button(composite,SWT.CENTER);
              button.setText(okText);
              button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
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
              button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
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
            for (String text_ : texts)
            {
              if (text_ != null)
              {
                textWidth = Math.max(textWidth,gc.textExtent(text_).x);
              }
            }
            gc.dispose();

            int column = 0;
            for (int i = 0; i < texts.length; i++)
            {
              if (texts[i] != null)
              {
                button = new Button(composite,SWT.CENTER);
                button.setEnabled((enabled == null) || enabled[i]);
                button.setText(texts[i]);
                button.setData(i);
                if (i == defaultValue) button.setFocus();
                button.setLayoutData(new TableLayoutData(0,column,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,textWidth+20,SWT.DEFAULT)); column++;
                button.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    Button widget = (Button)selectionEvent.widget;

                    value[0] = (Integer)widget.getData();
                    close(dialog,true);
                  }
                });
              }
            }
          }
        }

        Boolean result = run(dialog,
                             false,
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
        return ((result != null) && result) ? value[0] : -1;
      }
      else
      {
        return defaultValue;
      }
    }
    else
    {
      return defaultValue;
    }
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param image image
   * @param extendedMessageTitle extended message title or null
   * @param extendedMessage extended message or null
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1) or -1
   */
  public static int select(Shell                     parentShell,
                           final BooleanFieldUpdater showAgainFieldFlag,
                           String                    title,
                           Image                     image,
                           String                    extendedMessageTitle,
                           String[]                  extendedMessage,
                           String                    message,
                           String[]                  texts,
                           String[]                  helpTexts,
                           boolean[]                 enabled,
                           int                       defaultValue
                          )
  {
    return select(parentShell,showAgainFieldFlag,title,image,extendedMessageTitle,extendedMessage,message,texts,helpTexts,enabled,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
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
  public static int select(Shell                     parentShell,
                           final BooleanFieldUpdater showAgainFieldFlag,
                           String                    title,
                           String                    message,
                           String[]                  texts,
                           String[]                  helpTexts,
                           boolean[]                 enabled,
                           String                    okText,
                           String                    cancelText,
                           int                       defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,showAgainFieldFlag,title,IMAGE,(String)null,(String[])null,message,texts,helpTexts,enabled,okText,cancelText,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param okText ok-text
   * @param cancelText cancel-text
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1) or -1
   */
  public static int select(Shell     parentShell,
                           String    title,
                           Image     image,
                           String    message,
                           String[]  texts,
                           String[]  helpTexts,
                           boolean[] enabled,
                           String    okText,
                           String    cancelText,
                           int       defaultValue)
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
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
  public static int select(Shell     parentShell,
                           String    title,
                           String    message,
                           String[]  texts,
                           String[]  helpTexts,
                           boolean[] enabled,
                           String    okText,
                           String    cancelText,
                           int       defaultValue)
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           Image               image,
                           String              message,
                           String[]            texts,
                           String[]            helpTexts,
                           boolean[]           enabled,
                           int                 defaultValue
                          )
  {
    return select(parentShell,showAgainFieldFlag,title,image,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           String              message,
                           String[]            texts,
                           String[]            helpTexts,
                           boolean[]           enabled,
                           int                 defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,showAgainFieldFlag,title,IMAGE,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell     parentShell,
                           String    title,
                           Image     image,
                           String    message,
                           String[]  texts,
                           String[]  helpTexts,
                           boolean[] enabled,
                           int       defaultValue
                          )
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
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
  public static int select(Shell     parentShell,
                           String    title,
                           String    message,
                           String[]  texts,
                           String[]  helpTexts,
                           boolean[] enabled,
                           int       defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,(String)null,(String[])null,message,texts,helpTexts,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           Image               image,
                           String              message,
                           String[]            texts,
                           boolean[]           enabled,
                           int                 defaultValue
                          )
  {
    return select(parentShell,showAgainFieldFlag,title,image,(String)null,(String[])null,message,texts,(String[])null,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           String              message,
                           String[]            texts,
                           boolean[]           enabled,
                           int                 defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,showAgainFieldFlag,title,IMAGE,(String)null,(String[])null,message,texts,(String[])null,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param enabled array with enabled flags
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell     parentShell,
                           String    title,
                           Image     image,
                           String    message,
                           String[]  texts,
                           boolean[] enabled,
                           int       defaultValue
                          )
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,(String)null,(String[])null,message,texts,(String[])null,(boolean[])null,(String)null,(String)null,defaultValue);
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
  public static int select(Shell     parentShell,
                           String    title,
                           String    message,
                           String[]  texts,
                           boolean[] enabled,
                           int       defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,(String)null,(String[])null,message,texts,(String[])null,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param imag eimage
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           Image               image,
                           String              message,
                           String[]            texts,
                           String[]            helpTexts,
                           int                 defaultValue
                          )
  {
    return select(parentShell,showAgainFieldFlag,title,image,message,texts,helpTexts,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           String              message,
                           String[]            texts,
                           String[]            helpTexts,
                           int                 defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,showAgainFieldFlag,title,IMAGE,message,texts,helpTexts,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param extendedMessageTitle extended message title or null
   * @param extendedMessage extended message or null
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           Image    image,
                           String   extendedMessageTitle,
                           String[] extendedMessage,
                           String   message,
                           String[] texts,
                           String[] helpTexts,
                           int      defaultValue
                          )
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,extendedMessageTitle,extendedMessage,message,texts,helpTexts,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           Image    image,
                           String   message,
                           String[] texts,
                           String[] helpTexts,
                           int      defaultValue
                          )
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,message,texts,helpTexts,(boolean[])null,defaultValue);
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
  public static int select(Shell    parentShell,
                           String   title,
                           String   message,
                           String[] texts,
                           String[] helpTexts,
                           int      defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,message,texts,helpTexts,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           Image               image,
                           String              message,
                           String[]            texts,
                           int                 defaultValue
                          )
  {
    return select(parentShell,showAgainFieldFlag,title,image,message,texts,(String[])null,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell               parentShell,
                           BooleanFieldUpdater showAgainFieldFlag,
                           String              title,
                           String              message,
                           String[]            texts,
                           int                 defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,showAgainFieldFlag,title,IMAGE,message,texts,(String[])null,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param extendedMessageTitle extended message title or null
   * @param extendedMessage extended message or null
   * @param message confirmation message
   * @param texts array with texts
   * @param helpTexts help texts or null
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           Image    image,
                           String   extendedMessageTitle,
                           String[] extendedMessage,
                           String   message,
                           String[] texts,
                           int      defaultValue
                          )
  {
    return select(parentShell,(BooleanFieldUpdater)null,title,image,extendedMessageTitle,extendedMessage,message,texts,(String[])null,(boolean[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param image image
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           Image    image,
                           String   message,
                           String[] texts,
                           int      defaultValue
                          )
  {
    return select(parentShell,title,image,message,texts,(String[])null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param okText ok-text
   * @param cancelText cancel-text
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           String   message,
                           String[] texts,
                           String   okText,
                           String   cancelText,
                           int      defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,(String)null,(String[])null,message,texts,(String[])null,(boolean[])null,okText,cancelText,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param extendedMessageTitle extended message title or null
   * @param extendedMessage extended message or null
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           String   extendedMessageTitle,
                           String[] extendedMessage,
                           String   message,
                           String[] texts,
                           int      defaultValue
                          )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    return select(parentShell,(BooleanFieldUpdater)null,title,IMAGE,extendedMessageTitle,extendedMessage,message,texts,(String[])null,(boolean[])null,(String)null,(String)null,defaultValue);
  }

  /** select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value (0..n-1)
   * @return selection index (0..n-1)
   */
  public static int select(Shell    parentShell,
                           String   title,
                           String   message,
                           String[] texts,
                           int      defaultValue
                          )
  {
    return select(parentShell,title,message,texts,(String)null,(String)null,defaultValue);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return selection
   */
  public static BitSet selectMulti(Shell                     parentShell,
                                   final BooleanFieldUpdater showAgainFieldFlag,
                                   String                    title,
                                   String                    message,
                                   String[]                  texts,
                                   String                    yesText,
                                   String                    noText,
                                   BitSet                    defaultValue
                                  )
  {
    final Image IMAGE = Widgets.loadImage(parentShell.getDisplay(),"question.png");

    Composite composite,subComposite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final BitSet value = new BitSet(texts.length);

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
          int i = 0;
          for (String text : texts)
          {
            button = new Button(subComposite,SWT.CHECK);
            button.setText(text);
            button.setData(value);
            if ((defaultValue != null) && defaultValue.get(i)) button.setFocus();
            button.setLayoutData(new TableLayoutData(i,0,TableLayoutData.W));
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                int    n      = (Integer)widget.getData();

                value.set(n,widget.getSelection());
              }
            });

            i++;
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
        button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,true);
          }
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(noText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,false);
          }
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      Boolean result = (Boolean)run(dialog,defaultValue);
      return (result != null) ? value : defaultValue;
    }
    else
    {
      return defaultValue;
    }
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param yesText yes-text
   * @param noText no-text
   * @param defaultValue default value
   * @return selection
   */
  public static BitSet selectMulti(Shell    parentShell,
                                   String   title,
                                   String   message,
                                   String[] texts,
                                   String   yesText,
                                   String   noText,
                                   BitSet   defaultValue
                                  )
  {
    return selectMulti(parentShell,(BooleanFieldUpdater)null,title,message,texts,yesText,noText,null);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param yesText yes-text
   * @param noText no-text
   * @return selection
   */
  public static BitSet selectMulti(Shell               parentShell,
                                   BooleanFieldUpdater showAgainFieldFlag,
                                   String              title,
                                   String              message,
                                   String[]            texts,
                                   String              yesText,
                                   String              noText
                                  )
  {
    return selectMulti(parentShell,showAgainFieldFlag,title,message,texts,yesText,noText,null);
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
  public static BitSet selectMulti(Shell    parentShell,
                                   String   title,
                                   String   message,
                                   String[] texts,
                                   String   yesText,
                                   String   noText
                                  )
  {
    return selectMulti(parentShell,(BooleanFieldUpdater)null,title,message,texts,yesText,noText,null);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param showAgainFieldFlag show again field updater or null
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @return selection
   */
  public static BitSet selectMulti(Shell               parentShell,
                                   BooleanFieldUpdater showAgainFieldFlag,
                                   String              title,
                                   String              message,
                                   String[]            texts
                                  )
  {
    return selectMulti(parentShell,showAgainFieldFlag,title,message,texts,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @return selection
   */
  public static BitSet selectMulti(Shell    parentShell,
                                   String   title,
                                   String   message,
                                   String[] texts
                                  )
  {
    return selectMulti(parentShell,(BooleanFieldUpdater)null,title,message,texts,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display (can be null)
   * @param name user name to display (can be null)
   * @param text1 text (can be null)
   * @param text2 text (can be null)
   * @param okText OK button text
   * @param CancelText cancel button text
   * @return name+password or null on cancel
   */
  public static String password(Shell        parentShell,
                                String       title,
                                String       message,
                                String       name,
                                String       text1,
                                final String text2,
                                String       okText,
                                String       cancelText
                               )
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
      final Text   widgetPassword1Hidden,widgetPassword2Hidden;
      final Text   widgetPassword1Visible;
      final Button widgetShowPassword;
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
        if (name != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(Dialogs.tr("Name")+":");
          label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

          text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.READ_ONLY);
          text.setBackground(composite.getBackground());
          text.setText(name);
          text.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));

          row1++;
        }

        label = new Label(composite,SWT.LEFT);
        label.setText((text1 != null) ? text1 : Dialogs.tr("Password")+":");
        label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

        widgetPassword1Hidden  = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
        widgetPassword1Hidden.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        widgetPassword1Visible = new Text(composite,SWT.LEFT|SWT.BORDER);
        widgetPassword1Visible.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        widgetPassword1Visible.setVisible(false);
        row1++;

        if (text2 != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text2);
          label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

          widgetPassword2Hidden  = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
          widgetPassword2Hidden.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
          row1++;
        }
        else
        {
          widgetPassword2Hidden  = null;
        }

        widgetShowPassword = new Button(composite,SWT.CHECK);
        widgetShowPassword.setText(tr("show password"));
        widgetShowPassword.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE));
        widgetShowPassword.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;

            if (widget.getSelection())
            {
              // show passwords
              widgetPassword1Visible.setText(widgetPassword1Hidden.getText());
              widgetPassword1Hidden.setVisible(false);
              widgetPassword1Visible.setVisible(true);

              if (widgetPassword2Hidden != null)
              {
                widgetPassword2Hidden.setEnabled(false);
                widgetPassword2Hidden.setText("");
              }
            }
            else
            {
              // hide passwords
              widgetPassword1Hidden.setText(widgetPassword1Visible.getText());
              widgetPassword1Hidden.setVisible(true);
              widgetPassword1Visible.setVisible(false);

              if (widgetPassword2Hidden != null)
              {
                widgetPassword2Hidden.setText(widgetPassword1Visible.getText());
                widgetPassword2Hidden.setEnabled(true);
              }
            }
          }
        });
      }
      row0++;

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(row0,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String password1 = widgetShowPassword.getSelection() ? widgetPassword1Visible.getText() : widgetPassword1Hidden.getText();
            if (widgetPassword2Hidden != null)
            {
              String password2 = widgetPassword2Hidden.getText();
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
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      SelectionListener selectionListener1 = new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          if (widgetPassword2Hidden != null)
          {
            if (widgetShowPassword.getSelection())
            {
              widgetOkButton.forceFocus();
            }
            else
            {
              widgetPassword2Hidden.forceFocus();
            }
          }
          else
          {
            widgetOkButton.forceFocus();
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      };
      widgetPassword1Hidden.addSelectionListener(selectionListener1);
      widgetPassword1Visible.addSelectionListener(selectionListener1);
      if (widgetPassword2Hidden != null)
      {
        SelectionListener selectionListener2 = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            widgetOkButton.forceFocus();
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        };
        widgetPassword2Hidden.addSelectionListener(selectionListener2);
      }

      ModifyListener modifyListener = new ModifyListener()
      {
        @Override
        public void modifyText(ModifyEvent modifyEvent)
        {
          String password1,password2;

          if (widgetShowPassword.getSelection() || (widgetPassword2Hidden == null))
          {
            widgetOkButton.setEnabled(true);
          }
          else
          {
            widgetOkButton.setEnabled(widgetPassword1Hidden.getText().equals(widgetPassword2Hidden.getText()));
          }
        }
      };
      widgetPassword1Hidden.addModifyListener(modifyListener);
      widgetPassword1Visible.addModifyListener(modifyListener);
      if (widgetPassword2Hidden != null)
      {
        widgetPassword2Hidden.addModifyListener(modifyListener);
      }

      if (widgetShowPassword.getSelection())
      {
        widgetPassword1Visible.setFocus();
      }
      else
      {
        widgetPassword1Hidden.setFocus();
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
   * @param message message to display
   * @param name name to display (can be null)
   * @param text1 text
   * @param text2 text (can be null)
   * @return password or null on cancel
   */
  public static String password(Shell  parentShell,
                                String title,
                                String message,
                                String name,
                                String text1,
                                String text2
                               )
  {
    return password(parentShell,title,message,name,text1,text2,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @param name name to display (can be null)
   * @param text1 text
   * @return password or null on cancel
   */
  public static String password(Shell  parentShell,
                                String title,
                                String message,
                                String name,
                                String text1
                               )
  {
    return password(parentShell,title,message,name,text1,(String)null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @return password or null on cancel
   */
  public static String password(Shell  parentShell,
                                String title,
                                String message,
                                String text
                               )
  {
    return password(parentShell,title,message,(String)null,text);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  public static String password(Shell  parentShell,
                                String title,
                                String message
                               )
  {
    return password(parentShell,title,message,(String)null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @return password or null on cancel
   */
  public static String password(Shell  parentShell,
                                String title
                               )
  {
    return password(parentShell,title,(String)null);
  }

  /** login dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display (can be null)
   * @param name name to display
   * @param text1 text (can be null)
   * @param text2 text (can be null)
   * @param okText OK button text
   * @param CancelText cancel button text
   * @return name+password or null on cancel
   */
  public static String[] login(Shell        parentShell,
                               String       title,
                               String       message,
                               String       name,
                               String       text1,
                               final String text2,
                               String       okText,
                               String       cancelText
                              )
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
      final Text   widgetName;
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

        label = new Label(composite,SWT.LEFT);
        label.setText(Dialogs.tr("Name")+":");
        label.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.W));

        widgetName = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
        if (name != null) widgetName.setText(name);
        widgetName.setLayoutData(new TableLayoutData(row1,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
        row1++;

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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String name      = widgetName.getText();
            String password1 = widgetPassword1.getText();
            if (text2 != null)
            {
              String password2 = widgetPassword2.getText();
              if (password1.equals(password2))
              {
                close(dialog,new String[]{name,password1});
              }
            }
            else
            {
              close(dialog,new String[]{name,password1});
            }
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetName.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetPassword1.forceFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetPassword1.addSelectionListener(new SelectionListener()
      {
        @Override
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
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      if (text2 != null)
      {
        widgetPassword2.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            widgetOkButton.forceFocus();
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      widgetName.setFocus();
      widgetName.setSelection(new Point(0,widgetName.getText().length()));
      return (String[])run(dialog,null);
    }
    else
    {
      return null;
    }
  }

  /** login dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @param name name
   * @param text1 text
   * @param text2 text (can be null)
   * @return name+password or null on cancel
   */
  public static String[] login(Shell  parentShell,
                               String title,
                               String message,
                               String name,
                               String text1,
                               String text2
                              )
  {
    return login(parentShell,title,message,name,text1,text2,Dialogs.tr("OK"),Dialogs.tr("Cancel"));
  }

  /** login dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message message to display
   * @param name name
   * @param text text
   * @return name+password or null on cancel
   */
  public static String[] login(Shell  parentShell,
                               String title,
                               String message,
                               String name,
                               String text
                              )
  {
    return login(parentShell,title,message,name,text,(String)null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  private static Object lastFile = null;
  public static <T extends File> String file(final Shell                  parentShell,
                                             final FileDialogTypes        type,
                                             String                       title,
                                             T                            oldFile,
                                             final String[]               fileExtensions,
                                             String                       defaultFileExtension,
                                             int                          flags,
                                             final ListDirectory<T>       listDirectory,
                                             final ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' images/directory.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_DIRECTORY_DATA_ARRAY =
    {
      (byte)0x89,(byte)0x50,(byte)0x4e,(byte)0x47,(byte)0x0d,(byte)0x0a,(byte)0x1a,(byte)0x0a,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x0d,(byte)0x49,(byte)0x48,(byte)0x44,(byte)0x52,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x10,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x0c,(byte)0x02,(byte)0x03,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x16,(byte)0x89,(byte)0xd5,(byte)0x12,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x01,(byte)0x73,(byte)0x52,(byte)0x47,(byte)0x42,(byte)0x00,(byte)0xae,(byte)0xce,(byte)0x1c,
      (byte)0xe9,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x09,(byte)0x50,(byte)0x4c,(byte)0x54,(byte)0x45,
      (byte)0x00,(byte)0xff,(byte)0xff,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0xf0,(byte)0xff,(byte)0x80,
      (byte)0xfc,(byte)0x46,(byte)0xa8,(byte)0x94,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x01,(byte)0x74,
      (byte)0x52,(byte)0x4e,(byte)0x53,(byte)0x00,(byte)0x40,(byte)0xe6,(byte)0xd8,(byte)0x66,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x01,(byte)0x62,(byte)0x4b,(byte)0x47,(byte)0x44,(byte)0x00,(byte)0x88,
      (byte)0x05,(byte)0x1d,(byte)0x48,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x09,(byte)0x70,(byte)0x48,
      (byte)0x59,(byte)0x73,(byte)0x00,(byte)0x00,(byte)0x0b,(byte)0x13,(byte)0x00,(byte)0x00,(byte)0x0b,
      (byte)0x13,(byte)0x01,(byte)0x00,(byte)0x9a,(byte)0x9c,(byte)0x18,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x07,(byte)0x74,(byte)0x49,(byte)0x4d,(byte)0x45,(byte)0x07,(byte)0xdc,(byte)0x02,(byte)0x0b,
      (byte)0x0b,(byte)0x12,(byte)0x02,(byte)0x9e,(byte)0x46,(byte)0x5c,(byte)0x8b,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x23,(byte)0x49,(byte)0x44,(byte)0x41,(byte)0x54,(byte)0x08,(byte)0xd7,(byte)0x63,
      (byte)0x60,(byte)0x0c,(byte)0x61,(byte)0x60,(byte)0x60,(byte)0x60,(byte)0x5b,(byte)0x09,(byte)0x24,
      (byte)0xa4,(byte)0x56,(byte)0x39,(byte)0x30,(byte)0x30,(byte)0x84,(byte)0x86,(byte)0x86,(byte)0x3a,
      (byte)0x30,(byte)0x64,(byte)0xad,(byte)0x5a,(byte)0x45,(byte)0x88,(byte)0x00,(byte)0xa9,(byte)0x03,
      (byte)0x00,(byte)0xdc,(byte)0x39,(byte)0x12,(byte)0x79,(byte)0xf5,(byte)0x66,(byte)0x35,(byte)0x0f,
      (byte)0x00,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,(byte)0xae,
      (byte)0x42,(byte)0x60,(byte)0x82
    };

    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' images/file.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_FILE_DATA_ARRAY =
    {
      (byte)0x89,(byte)0x50,(byte)0x4e,(byte)0x47,(byte)0x0d,(byte)0x0a,(byte)0x1a,(byte)0x0a,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x0d,(byte)0x49,(byte)0x48,(byte)0x44,(byte)0x52,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x0c,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x0c,(byte)0x02,(byte)0x03,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x2b,(byte)0x1b,(byte)0xb4,(byte)0x74,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x01,(byte)0x73,(byte)0x52,(byte)0x47,(byte)0x42,(byte)0x00,(byte)0xae,(byte)0xce,(byte)0x1c,
      (byte)0xe9,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x09,(byte)0x50,(byte)0x4c,(byte)0x54,(byte)0x45,
      (byte)0xff,(byte)0xff,(byte)0xff,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0xff,(byte)0xff,(byte)0xf3,
      (byte)0x77,(byte)0x59,(byte)0xc3,(byte)0x64,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x01,(byte)0x74,
      (byte)0x52,(byte)0x4e,(byte)0x53,(byte)0x00,(byte)0x40,(byte)0xe6,(byte)0xd8,(byte)0x66,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x01,(byte)0x62,(byte)0x4b,(byte)0x47,(byte)0x44,(byte)0x00,(byte)0x88,
      (byte)0x05,(byte)0x1d,(byte)0x48,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x09,(byte)0x70,(byte)0x48,
      (byte)0x59,(byte)0x73,(byte)0x00,(byte)0x00,(byte)0x0b,(byte)0x13,(byte)0x00,(byte)0x00,(byte)0x0b,
      (byte)0x13,(byte)0x01,(byte)0x00,(byte)0x9a,(byte)0x9c,(byte)0x18,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x07,(byte)0x74,(byte)0x49,(byte)0x4d,(byte)0x45,(byte)0x07,(byte)0xdb,(byte)0x01,(byte)0x0b,
      (byte)0x01,(byte)0x21,(byte)0x06,(byte)0x3a,(byte)0x72,(byte)0x32,(byte)0x1c,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x19,(byte)0x49,(byte)0x44,(byte)0x41,(byte)0x54,(byte)0x08,(byte)0xd7,(byte)0x63,
      (byte)0x10,(byte)0x0d,(byte)0x75,(byte)0x60,(byte)0x90,(byte)0x5a,(byte)0x05,(byte)0xc2,(byte)0x21,
      (byte)0x40,(byte)0xbc,(byte)0x04,(byte)0x2f,(byte)0x16,(byte)0x0d,(byte)0x0d,(byte)0x01,(byte)0x00,
      (byte)0x50,(byte)0x65,(byte)0x0e,(byte)0xc5,(byte)0x87,(byte)0x85,(byte)0xab,(byte)0x0c,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,(byte)0xae,(byte)0x42,
      (byte)0x60,(byte)0x82
    };

    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' folderUp.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_FOLDER_UP_DATA_ARRAY =
    {
      (byte)0x89,(byte)0x50,(byte)0x4e,(byte)0x47,(byte)0x0d,(byte)0x0a,(byte)0x1a,(byte)0x0a,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x0d,(byte)0x49,(byte)0x48,(byte)0x44,(byte)0x52,(byte)0x00,(byte)0x00,
      (byte)0x00,(byte)0x10,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x10,(byte)0x08,(byte)0x06,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x1f,(byte)0xf3,(byte)0xff,(byte)0x61,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x06,(byte)0x62,(byte)0x4b,(byte)0x47,(byte)0x44,(byte)0x00,(byte)0xff,(byte)0x00,(byte)0xff,
      (byte)0x00,(byte)0xff,(byte)0xa0,(byte)0xbd,(byte)0xa7,(byte)0x93,(byte)0x00,(byte)0x00,(byte)0x00,
      (byte)0x09,(byte)0x70,(byte)0x48,(byte)0x59,(byte)0x73,(byte)0x00,(byte)0x00,(byte)0x0e,(byte)0xc4,
      (byte)0x00,(byte)0x00,(byte)0x0e,(byte)0xc4,(byte)0x01,(byte)0x95,(byte)0x2b,(byte)0x0e,(byte)0x1b,
      (byte)0x00,(byte)0x00,(byte)0x00,(byte)0x07,(byte)0x74,(byte)0x49,(byte)0x4d,(byte)0x45,(byte)0x07,
      (byte)0xe0,(byte)0x05,(byte)0x1b,(byte)0x0d,(byte)0x0f,(byte)0x30,(byte)0x3c,(byte)0xf1,(byte)0x51,
      (byte)0xfc,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x19,(byte)0x74,(byte)0x45,(byte)0x58,(byte)0x74,
      (byte)0x43,(byte)0x6f,(byte)0x6d,(byte)0x6d,(byte)0x65,(byte)0x6e,(byte)0x74,(byte)0x00,(byte)0x43,
      (byte)0x72,(byte)0x65,(byte)0x61,(byte)0x74,(byte)0x65,(byte)0x64,(byte)0x20,(byte)0x77,(byte)0x69,
      (byte)0x74,(byte)0x68,(byte)0x20,(byte)0x47,(byte)0x49,(byte)0x4d,(byte)0x50,(byte)0x57,(byte)0x81,
      (byte)0x0e,(byte)0x17,(byte)0x00,(byte)0x00,(byte)0x02,(byte)0x3c,(byte)0x49,(byte)0x44,(byte)0x41,
      (byte)0x54,(byte)0x38,(byte)0xcb,(byte)0xcd,(byte)0x92,(byte)0x4d,(byte)0x48,(byte)0x54,(byte)0x61,
      (byte)0x14,(byte)0x86,(byte)0x9f,(byte)0xef,(byte)0xbb,(byte)0x3f,(byte)0xce,(byte)0x94,(byte)0xa3,
      (byte)0x86,(byte)0x15,(byte)0x66,(byte)0x39,(byte)0x42,(byte)0x14,(byte)0xa8,(byte)0x25,(byte)0x99,
      (byte)0xd5,(byte)0x48,(byte)0x96,(byte)0x41,(byte)0xd4,(byte)0xa2,(byte)0xda,(byte)0xb9,(byte)0x6c,
      (byte)0x55,(byte)0x8b,(byte)0x16,(byte)0x61,(byte)0x8b,(byte)0x56,(byte)0x2e,(byte)0x0b,(byte)0x5b,
      (byte)0x54,(byte)0x14,(byte)0x6e,(byte)0x5a,(byte)0x44,(byte)0x2d,(byte)0x8a,(byte)0x16,(byte)0xd1,
      (byte)0x26,(byte)0x82,(byte)0xa0,(byte)0x76,(byte)0x15,(byte)0x91,(byte)0x14,(byte)0x51,(byte)0x44,
      (byte)0xa1,(byte)0x12,(byte)0x41,(byte)0x89,(byte)0xa2,(byte)0x29,(byte)0xa5,(byte)0xf9,(byte)0xc7,
      (byte)0x38,(byte)0xcd,(byte)0xcc,(byte)0x9d,(byte)0x7b,(byte)0xbf,(byte)0xfb,(byte)0x9d,(byte)0x16,
      (byte)0x03,(byte)0x29,(byte)0x2d,(byte)0xda,(byte)0xd6,(byte)0xbb,(byte)0x3c,(byte)0x87,(byte)0xf7,
      (byte)0xe1,(byte)0x1c,(byte)0xde,(byte)0x17,(byte)0xfe,(byte)0xb5,(byte)0x14,(byte)0xc0,(byte)0xe7,
      (byte)0xc7,(byte)0xbb,(byte)0x06,(byte)0x2b,(byte)0xaa,(byte)0xb7,(byte)0xb5,(byte)0x28,(byte)0xa5,
      (byte)0x57,(byte)0xac,(byte)0x84,(byte)0xd2,(byte)0xd2,(byte)0xe8,(byte)0x64,(byte)0xb1,(byte)0xf4,
      (byte)0x7d,(byte)0x6f,(byte)0x64,(byte)0xa4,(byte)0xf4,(byte)0xa7,(byte)0x31,(byte)0xf7,(byte)0x69,
      (byte)0x2c,(byte)0x7f,(byte)0xb0,(byte)0x0f,(byte)0xe3,(byte)0x96,(byte)0x29,(byte)0xba,(byte)0x29,
      (byte)0xbd,(byte)0xff,(byte)0x8e,(byte)0xa7,(byte)0x94,(byte)0x02,(byte)0x29,(byte)0x9b,(byte)0x51,
      (byte)0x0a,(byte)0x13,(byte)0xcc,(byte)0x6d,(byte)0x9e,(byte)0x1e,(byte)0xbc,(byte)0x30,(byte)0x29,
      (byte)0x52,(byte)0x9e,(byte)0x02,(byte)0x38,(byte)0x7e,(byte)0x95,(byte)0x60,(byte)0xa3,(byte)0x1f,
      (byte)0x95,(byte)0xde,(byte)0xd3,(byte)0x5e,(byte)0x18,(byte)0xba,(byte)0xef,(byte)0x02,(byte)0x10,
      (byte)0x49,(byte)0x80,(byte)0x0d,(byte)0x2b,(byte)0x4a,(byte)0xd3,(byte)0x37,(byte)0x88,(byte)0x43,
      (byte)0x07,(byte)0x74,(byte)0x15,(byte)0x4a,(byte)0xa5,(byte)0x48,(byte)0xd4,(byte)0x1f,(byte)0x62,
      (byte)0x63,(byte)0xe6,(byte)0x8a,(byte)0x2f,(byte)0xd6,(byte)0xd2,(byte)0x7a,(byte)0xa9,(byte)0x96,
      (byte)0x77,(byte)0xbd,(byte)0x13,(byte)0x24,(byte)0xbd,(byte)0x04,(byte)0x4a,(byte)0x57,(byte)0xd6,
      (byte)0x8d,(byte)0x4c,(byte)0x0d,(byte)0x9c,(byte)0x07,(byte)0xee,(byte)0x6b,(byte)0x00,(byte)0x31,
      (byte)0x5a,(byte)0x8b,(byte)0x0d,(byte)0x89,(byte)0x43,(byte)0x1f,(byte)0xb1,(byte)0x9a,(byte)0x38,
      (byte)0xff,(byte)0x95,(byte)0xe2,(byte)0xcc,(byte)0x73,(byte)0xe6,(byte)0x06,(byte)0xcf,(byte)0x91,
      (byte)0xfd,(byte)0x74,(byte)0x8d,(byte)0x7d,(byte)0xfd,(byte)0xeb,(byte)0x69,(byte)0x6f,(byte)0xec,
      (byte)0xe0,(byte)0x68,(byte)0xff,(byte)0x16,(byte)0x66,(byte)0x86,(byte)0xfa,(byte)0x30,(byte)0xe1,
      (byte)0xa2,(byte)0x2f,(byte)0x62,(byte)0x1b,(byte)0x01,(byte)0xca,(byte)0x17,(byte)0x60,(byte)0x13,
      (byte)0xd6,(byte)0x14,(byte)0x09,(byte)0x0b,(byte)0x20,(byte)0x61,(byte)0x9e,(byte)0x28,(byte)0xb7,
      (byte)0x44,(byte)0xaa,(byte)0xe9,(byte)0x04,(byte)0x89,(byte)0xda,(byte)0x66,(byte)0x8e,(byte)0x5d,
      (byte)0x6f,(byte)0xa2,(byte)0x2d,(byte)0x7d,(byte)0x80,(byte)0xb6,(byte)0x86,(byte)0x2e,(byte)0x3e,
      (byte)0x78,(byte)0x95,(byte)0x9c,(byte)0x7c,(byte)0xfd,(byte)0x8c,(byte)0x47,(byte)0x3b,(byte)0xae,
      (byte)0xa2,(byte)0x62,(byte)0xc7,(byte)0x5d,(byte)0x06,(byte)0x44,(byte)0x1a,(byte)0x37,(byte)0xb9,
      (byte)0x9e,(byte)0x9a,(byte)0xe6,(byte)0x53,(byte)0x80,(byte)0x05,(byte)0x11,(byte)0x04,(byte)0xcb,
      (byte)0xf1,(byte)0x5b,(byte)0x7b,(byte)0xd8,(byte)0x5e,(byte)0xbf,(byte)0x87,(byte)0xd6,(byte)0x4d,
      (byte)0xfb,(byte)0x98,(byte)0x58,(byte)0x18,(byte)0x65,(byte)0x67,(byte)0x43,(byte)0x27,(byte)0xc6,
      (byte)0x5a,(byte)0xba,(byte)0x6f,(byte)0xee,(byte)0xa2,(byte)0xbf,(byte)0x2a,(byte)0xc9,(byte)0x32,
      (byte)0xc0,(byte)0x88,(byte)0x48,(byte)0x1c,(byte)0x60,(byte)0x0b,(byte)0xc3,(byte)0x48,(byte)0x9c,
      (byte)0x45,(byte)0xe2,(byte)0x1c,(byte)0x67,(byte)0x1e,(byte)0x5f,(byte)0xa4,(byte)0x65,(byte)0x63,
      (byte)0x3b,(byte)0xcd,(byte)0x75,(byte)0x1d,(byte)0x7c,(byte)0xcb,(byte)0x4e,(byte)0x62,(byte)0x6d,
      (byte)0xc8,(byte)0xc2,(byte)0xcf,(byte)0x69,(byte)0x76,(byte)0xa7,(byte)0x3b,(byte)0x79,(byte)0x6b,
      (byte)0x5f,(byte)0x72,(byte)0xe4,(byte)0xcb,(byte)0x8b,(byte)0x04,(byte)0x80,(byte)0x06,(byte)0x50,
      (byte)0x01,(byte)0x22,(byte)0xd6,(byte)0x60,(byte)0xa3,(byte)0x59,(byte)0x6c,(byte)0x38,(byte)0x8b,
      (byte)0x0d,(byte)0x67,(byte)0x58,(byte)0x93,(byte)0x5c,(byte)0xcd,(byte)0x7c,(byte)0xf6,(byte)0x0b,
      (byte)0xc3,(byte)0x53,(byte)0xaf,(byte)0xa8,(byte)0xab,(byte)0xde,(byte)0x84,(byte)0xe7,(byte)0x78,
      (byte)0x6c,(byte)0xa8,(byte)0x4e,(byte)0xf3,(byte)0x66,(byte)0xec,(byte)0x09,(byte)0x23,(byte)0x33,
      (byte)0xef,(byte)0xa9,(byte)0x71,(byte)0xcb,(byte)0xc9,(byte)0x94,(byte)0x83,(byte)0x8f,(byte)0x10,
      (byte)0x45,(byte)0x8c,(byte)0x98,(byte)0x05,(byte)0xc4,(byte)0xcc,(byte)0x23,(byte)0xd1,(byte)0x1c,
      (byte)0x7d,(byte)0x99,(byte)0x0c,(byte)0x97,(byte)0x33,(byte)0x3b,(byte)0xf8,(byte)0xbe,(byte)0x38,
      (byte)0x82,(byte)0x46,(byte)0xe1,(byte)0x6a,(byte)0x0f,(byte)0x47,(byte)0x69,(byte)0x46,(byte)0x67,
      (byte)0x3f,(byte)0x32,(byte)0x70,(byte)0xfa,(byte)0x09,(byte)0x0f,(byte)0xd6,(byte)0xb5,(byte)0x97,
      (byte)0x7e,(byte)0xbf,(byte)0x60,(byte)0x43,(byte)0xbb,(byte)0x68,(byte)0xf2,(byte)0xf3,(byte)0xab,
      (byte)0xa2,(byte)0x9f,(byte)0x5e,(byte)0x0a,(byte)0x9b,(byte)0xd2,(byte)0x22,(byte)0x1e,(byte)0xd8,
      (byte)0x14,(byte)0xc8,(byte)0x5a,(byte)0x7c,(byte)0x77,(byte)0x08,(byte)0x47,(byte)0x3b,(byte)0x78,
      (byte)0xda,(byte)0xc5,(byte)0xd5,(byte)0x0e,(byte)0x49,(byte)0xd7,(byte)0x27,(byte)0x0e,(byte)0x02,
      (byte)0x91,(byte)0x92,(byte)0x5d,(byte)0x01,(byte)0x28,(byte)0x04,(byte)0x57,(byte)0x47,(byte)0x6f,
      (byte)0x76,(byte)0x67,(byte)0x94,(byte)0x70,(byte)0x18,(byte)0x25,(byte)0xa9,(byte)0x95,(byte)0x8d,
      (byte)0xf3,(byte)0x1b,(byte)0x2a,(byte)0x13,(byte)0x8e,(byte)0xd2,(byte)0x34,(byte)0xd6,(byte)0x6e,
      (byte)0xc5,(byte)0xc4,(byte)0x06,(byte)0x5f,(byte)0xf9,(byte)0x8c,(byte)0xdf,(byte)0xee,(byte)0xc9,
      (byte)0x89,(byte)0xc8,(byte)0xdd,(byte)0xdf,(byte)0x55,(byte)0xfe,(byte)0x9b,(byte)0xce,(byte)0xde,
      (byte)0xeb,(byte)0x12,(byte)0x63,(byte)0x43,(byte)0x02,(byte)0x13,(byte)0x12,(byte)0x98,(byte)0x80,
      (byte)0x42,(byte)0x54,(byte)0xb4,(byte)0x0f,(byte)0x7b,(byte)0xc6,(byte)0x1d,(byte)0xfe,(byte)0x1b,
      (byte)0xfd,(byte)0x02,(byte)0x33,(byte)0x62,(byte)0x06,(byte)0x87,(byte)0x0b,(byte)0xc4,(byte)0xec,
      (byte)0xbb,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,
      (byte)0xae,(byte)0x42,(byte)0x60,(byte)0x82
    };
    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' folderNew.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_FOLDER_NEW_DATA_ARRAY =
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
      (byte)0x79,(byte)0x71,(byte)0xc9,(byte)0x65,(byte)0x3c,(byte)0x00,(byte)0x00,(byte)0x02,(byte)0x2e,
      (byte)0x49,(byte)0x44,(byte)0x41,(byte)0x54,(byte)0x18,(byte)0x19,(byte)0xa5,(byte)0xc1,(byte)0xcd,
      (byte)0x8b,(byte)0x4d,(byte)0x61,(byte)0x00,(byte)0xc7,(byte)0xf1,(byte)0xef,(byte)0x73,(byte)0xe6,
      (byte)0xcc,(byte)0x75,(byte)0x94,(byte)0x3b,(byte)0x44,(byte)0x26,(byte)0x6e,(byte)0x23,(byte)0xa1,
      (byte)0xc9,(byte)0x0c,(byte)0x99,(byte)0x31,(byte)0x85,(byte)0x86,(byte)0x46,(byte)0x16,(byte)0xd8,
      (byte)0x49,(byte)0xf9,(byte)0x0b,(byte)0xa4,(byte)0xac,(byte)0x18,(byte)0xa5,(byte)0x24,(byte)0x53,
      (byte)0x4a,(byte)0x29,(byte)0x0b,(byte)0xc4,(byte)0x4e,(byte)0xc8,(byte)0xc6,(byte)0xca,(byte)0xc6,
      (byte)0x4a,(byte)0x62,(byte)0x41,(byte)0x1a,(byte)0xd4,(byte)0x90,(byte)0x9d,(byte)0x77,(byte)0xc9,
      (byte)0xe4,(byte)0x25,(byte)0x2f,(byte)0x83,(byte)0xe1,(byte)0xba,(byte)0xf7,(byte)0xdc,(byte)0x79,
      (byte)0xbb,(byte)0xf7,(byte)0x9c,(byte)0xe7,(byte)0x79,(byte)0x7e,(byte)0xce,(byte)0x29,(byte)0x45,
      (byte)0x36,(byte)0xd4,(byte)0x7c,(byte)0x3e,(byte)0x46,(byte)0x12,(byte)0xd3,(byte)0x11,(byte)0x30,
      (byte)0x4d,(byte)0x01,(byte)0xd3,(byte)0x14,(byte)0x92,(byte)0x79,(byte)0x75,(byte)0x7d,(byte)0xed,
      (byte)0xa3,(byte)0xa8,(byte)0x65,(byte)0xc5,(byte)0x0a,(byte)0x63,(byte)0x02,(byte)0x7e,(byte)0x13,
      (byte)0x8d,(byte)0xb1,(byte)0xd7,(byte)0x1f,(byte)0x9c,(byte)0xad,(byte)0xae,(byte)0x07,(byte)0x1a,
      (byte)0xfc,(byte)0xc9,(byte)0x90,(byte)0x9b,(byte)0x58,(byte)0xbe,(byte)0xed,(byte)0xa9,(byte)0x0d,
      (byte)0xc9,(byte)0x18,(byte)0x05,(byte)0x1d,(byte)0x8b,(byte)0xfa,(byte)0x2e,(byte)0x36,(byte)0x1b,
      (byte)0x63,(byte)0x40,(byte)0x64,(byte)0x04,(byte)0xc6,(byte)0x60,(byte)0xeb,(byte)0xdf,(byte)0x97,
      (byte)0x7e,(byte)0x7d,(byte)0x74,(byte)0xf4,(byte)0x83,(byte)0x40,(byte)0xfc,(byte)0xd2,(byte)0x54,
      (byte)0x68,(byte)0x91,(byte)0x5c,(byte)0x3a,(byte)0x3a,(byte)0xfe,(byte)0x79,(byte)0xf0,(byte)0x20,
      (byte)0x70,(byte)0x39,(byte)0x24,(byte)0x67,(byte)0x55,(byte)0xc7,(byte)0x27,(byte)0x33,(byte)0x1a,
      (byte)0x5f,(byte)0xce,(byte)0xe1,(byte)0x92,(byte)0x26,(byte)0x08,(byte)0x5a,(byte)0x30,(byte)0xa6,
      (byte)0x48,(byte)0x54,(byte)0xda,(byte)0x4c,(byte)0x69,(byte)0xdd,(byte)0x89,(byte)0x02,(byte)0x12,
      (byte)0xe0,(byte)0x41,(byte)0x1e,(byte)0x10,(byte)0x26,(byte)0x98,(byte)0xb5,(byte)0x60,(byte)0xf8,
      (byte)0x6a,(byte)0xdf,(byte)0x11,(byte)0xe0,(byte)0x72,(byte)0x48,(byte)0x2e,(byte)0x35,(byte)0x81,
      (byte)0x7c,(byte)0x82,(byte)0x4b,(byte)0x0a,(byte)0xc8,(byte)0x0b,(byte)0x3f,(byte)0xf5,(byte)0x9e,
      (byte)0x74,(byte)0xe2,(byte)0x2b,(byte)0xe3,(byte)0xa3,(byte)0x77,(byte)0x68,(byte)0x8e,(byte)0x16,
      (byte)0x82,(byte)0x52,(byte)0xf0,(byte)0x09,(byte)0x92,(byte)0x05,(byte)0x2c,(byte)0xc5,(byte)0x8e,
      (byte)0x43,(byte)0x05,(byte)0xe4,(byte)0x17,(byte)0x93,(byte)0x09,(byte)0xc9,(byte)0x19,(byte)0x45,
      (byte)0xde,(byte)0x4e,(byte)0x91,(byte)0x4c,(byte)0x82,(byte)0x92,(byte)0x09,(byte)0xd2,(byte)0xb1,
      (byte)0x1a,(byte)0xc5,(byte)0x8e,(byte)0x9d,(byte)0x44,(byte)0xf3,(byte)0x3a,(byte)0x01,(byte)0x03,
      (byte)0x08,(byte)0xf0,(byte)0x20,(byte)0x01,(byte)0xc2,(byte)0x84,(byte)0x73,(byte)0xc0,(byte)0x05,
      (byte)0x21,(byte)0x99,(byte)0x90,(byte)0x5c,(byte)0x1a,(byte)0x10,(byte)0xce,(byte)0x6c,(byte)0x65,
      (byte)0x4e,(byte)0xe7,(byte)0x6e,(byte)0xc0,(byte)0x83,(byte)0x84,(byte)0xf0,(byte)0xf8,(byte)0xe4,
      (byte)0x13,(byte)0xb6,(byte)0x7a,(byte)0x0b,(byte)0xb9,(byte)0x18,(byte)0xd9,(byte)0x18,(byte)0xd9,
      (byte)0x1a,(byte)0x72,(byte)0xe3,(byte)0x44,(byte)0x6d,(byte)0x87,(byte)0xa1,(byte)0x21,(byte)0x72,
      (byte)0x21,(byte)0xb9,(byte)0x54,(byte)0x92,(byte)0xab,(byte)0xe3,(byte)0x27,(byte)0x9f,(byte)0x20,
      (byte)0x17,(byte)0x23,(byte)0x37,(byte)0x86,(byte)0x6c,(byte)0x15,(byte)0x9f,(byte)0x56,(byte)0x90,
      (byte)0x8b,(byte)0xc1,(byte)0x56,(byte)0x19,(byte)0x1c,(byte)0xae,(byte)0x70,(byte)0xf7,(byte)0x9d,
      (byte)0x23,(byte)0x9e,(byte)0x9a,(byte)0x22,(byte)0xb5,(byte)0xfb,(byte)0xe9,(byte)0x09,(byte)0x5c,
      (byte)0xd8,(byte)0x0e,(byte)0x84,(byte)0xe4,(byte)0x1a,(byte)0x48,(byte)0xde,(byte)0xe2,(byte)0xd3,
      (byte)0x6f,(byte)0xc8,(byte)0xc6,(byte)0xc8,(byte)0xc5,(byte)0xc8,(byte)0x56,(byte)0x91,(byte)0x8d,
      (byte)0x91,(byte)0xab,(byte)0x70,(byte)0xe3,(byte)0x65,(byte)0x85,(byte)0xa7,(byte)0xf5,(byte)0x22,
      (byte)0x9b,(byte)0x36,(byte)0xae,(byte)0xa1,(byte)0x6d,(byte)0x6e,(byte)0x3b,(byte)0xb7,(byte)0x9f,
      (byte)0x5f,(byte)0xe1,(byte)0xde,(byte)0xb3,(byte)0x91,(byte)0xf0,(byte)0x52,(byte)0x7f,(byte)0xe9,
      (byte)0x54,(byte)0x40,(byte)0x2e,(byte)0x91,(byte)0x0c,(byte)0x0e,(byte)0xd9,(byte)0x1f,(byte)0xc8,
      (byte)0x96,(byte)0x51,(byte)0xfa,(byte)0x1d,(byte)0xd9,(byte)0x32,(byte)0xb2,(byte)0x65,(byte)0x64,
      (byte)0x2b,(byte)0x5c,(byte)0x7b,(byte)0x51,(byte)0x65,(byte)0x75,(byte)0x47,(byte)0x37,(byte)0x2e,
      (byte)0x70,(byte)0x74,(byte)0x2f,(byte)0xdc,(byte)0x8a,(byte)0x33,(byte)0x29,(byte)0xbd,(byte)0xab,
      (byte)0x36,(byte)0x90,(byte)0xd9,(byte)0x1b,(byte)0x90,(byte)0x51,(byte)0xe2,(byte)0x2b,(byte)0x76,
      (byte)0xb2,(byte)0x1c,(byte)0xa7,(byte)0xe3,(byte)0x05,(byte)0x6f,(byte)0x27,(byte)0x8b,(byte)0xd8,
      (byte)0xc6,(byte)0x7c,(byte)0x5c,(byte)0xd2,(byte)0x86,(byte)0x77,(byte)0xcb,(byte)0xf0,(byte)0x6e,
      (byte)0x25,(byte)0xa3,(byte)0xb5,(byte)0x32,(byte)0xcd,(byte)0x66,(byte)0x16,(byte)0xdb,(byte)0x3b,
      (byte)0xf7,(byte)0x91,(byte)0x3b,(byte)0xb0,(byte)0xf9,(byte)0x02,(byte)0xcb,(byte)0x5a,(byte)0xbb,
      (byte)0xc8,(byte)0x44,(byte)0x21,(byte)0x19,(byte)0xd5,(byte)0x1b,(byte)0x27,(byte)0xdf,(byte)0x9c,
      (byte)0xdf,(byte)0xb1,(byte)0x0e,(byte)0xb1,(byte)0x05,(byte)0x43,(byte)0x91,(byte)0xbf,(byte)0xd4,
      (byte)0x6a,(byte)0x95,(byte)0xe8,(byte)0xf9,(byte)0xc8,(byte)0x7d,(byte)0x1e,(byte)0x8f,(byte)0x0c,
      (byte)0x31,(byte)0xb0,(byte)0xf5,(byte)0x22,(byte)0xc7,(byte)0x6f,(byte)0xee,(byte)0x22,(byte)0x6a,
      (byte)0x9a,(byte)0x41,(byte)0xa6,(byte)0x6e,(byte)0x24,(byte)0xf1,(byte)0x2f,(byte)0xbd,(byte)0xfd,
      (byte)0xa5,(byte)0x63,(byte)0x6d,(byte)0x4b,(byte)0x66,(byte)0x1f,(byte)0xde,(byte)0xd0,(byte)0xd5,
      (byte)0x47,(byte)0xfb,(byte)0x82,(byte)0x1e,(byte)0x86,(byte)0xbf,(byte)0x3c,(byte)0xe4,(byte)0xfe,
      (byte)0x93,(byte)0x21,(byte)0x3e,(byte)0xbe,(byte)0x8d,(byte)0x4f,(byte)0x1b,(byte)0x49,(byte)0xfc,
      (byte)0x8f,(byte)0xde,(byte)0xfe,(byte)0xd2,(byte)0x71,(byte)0x60,(byte)0x0f,(byte)0x50,(byte)0x04,
      (byte)0xc6,(byte)0x80,(byte)0xb3,(byte)0x0f,(byte)0xce,(byte)0x8c,(byte)0x0c,(byte)0xfc,(byte)0x04,
      (byte)0xff,(byte)0x73,(byte)0x2d,(byte)0xfb,(byte)0xb7,(byte)0xf0,(byte)0xeb,(byte)0x4d,(byte)0x00,
      (byte)0x00,(byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,(byte)0xae,(byte)0x42,
      (byte)0x60,(byte)0x82
    };
    // create: hexdump -v -e '1/1 "(byte)0x%02x" "\n"' delete.png | awk 'BEGIN {n=0;} /.*/ { if (n > 8) { printf("\n"); n=0; }; f=1; printf("%s,",$1); n++; }'
    final byte[] IMAGE_DELETE_DATA_ARRAY =
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
      (byte)0x79,(byte)0x71,(byte)0xc9,(byte)0x65,(byte)0x3c,(byte)0x00,(byte)0x00,(byte)0x02,(byte)0x5d,
      (byte)0x49,(byte)0x44,(byte)0x41,(byte)0x54,(byte)0x38,(byte)0xcb,(byte)0xa5,(byte)0x93,(byte)0xfb,
      (byte)0x4b,(byte)0x53,(byte)0x61,(byte)0x18,(byte)0xc7,(byte)0xfd,(byte)0x5b,(byte)0xb6,(byte)0x1f,
      (byte)0xa2,(byte)0x04,(byte)0x89,(byte)0x6e,(byte)0x84,(byte)0x84,(byte)0x51,(byte)0x50,(byte)0x98,
      (byte)0xf3,(byte)0x32,(byte)0x77,(byte)0xd6,(byte)0xda,(byte)0xdc,(byte)0xa6,(byte)0xce,(byte)0xb3,
      (byte)0x4c,(byte)0x8f,(byte)0x5b,(byte)0x2c,(byte)0x62,(byte)0x69,(byte)0x8e,(byte)0x9d,(byte)0x61,
      (byte)0x9a,(byte)0x41,(byte)0x8d,(byte)0xc5,(byte)0x5c,(byte)0xf8,(byte)0x43,(byte)0xa8,(byte)0xa5,
      (byte)0x76,(byte)0xd5,(byte)0xca,(byte)0x5f,(byte)0x32,(byte)0x4d,(byte)0x6c,(byte)0x94,(byte)0x5a,
      (byte)0x46,(byte)0x6a,(byte)0xd7,(byte)0xa1,(byte)0xe5,(byte)0xb1,(byte)0xf2,(byte)0xd2,(byte)0x4e,
      (byte)0x4d,(byte)0x6a,(byte)0x6d,(byte)0x9e,(byte)0xb3,(byte)0x6b,(byte)0xca,(byte)0xb7,(byte)0xb9,
      (byte)0x60,(byte)0x26,(byte)0x2e,(byte)0x23,(byte)0x7a,(byte)0xe1,(byte)0xfb,(byte)0xcb,(byte)0xcb,
      (byte)0xfb,(byte)0xf9,(byte)0xbc,(byte)0x3c,(byte)0x0f,(byte)0xcf,(byte)0x93,(byte)0x02,(byte)0x20,
      (byte)0xe5,(byte)0x7f,(byte)0xb2,(byte)0xe6,(byte)0x62,(byte)0x56,(byte)0xaf,(byte)0x17,(byte)0xce,
      (byte)0x50,(byte)0x15,(byte)0xe6,(byte)0xf7,(byte)0x54,(byte)0x19,(byte)0x33,(byte)0xa5,(byte)0x25,
      (byte)0xb9,(byte)0x49,(byte)0xad,(byte)0x86,(byte)0x7b,(byte)0x47,(byte)0xaa,(byte)0x99,(byte)0x71,
      (byte)0x52,(byte)0x69,(byte)0x76,(byte)0x95,(byte)0xc8,(byte)0x85,(byte)0xeb,(byte)0x0a,(byte)0xe6,
      (byte)0x74,(byte)0x7a,(byte)0xe2,(byte)0x23,(byte)0x45,(byte)0xb1,(byte)0xdf,(byte)0x1c,(byte)0x36,
      (byte)0x84,(byte)0x07,(byte)0x9d,(byte)0x88,(byte)0xbc,(byte)0x19,(byte)0x45,(byte)0x64,(byte)0x64,
      (byte)0x08,(byte)0xfc,(byte)0xdd,(byte)0x0e,(byte)0xcc,(byte)0x1a,(byte)0x4a,(byte)0xf1,(byte)0xaa,
      (byte)0x90,(byte)0x60,(byte)0x9f,(byte)0xab,(byte)0xc5,(byte)0x44,(byte)0x52,(byte)0xc1,(byte)0x32,
      (byte)0x3c,(byte)0x5d,(byte)0x4e,(byte)0xf1,(byte)0x0b,(byte)0xb7,(byte)0x3b,(byte)0xb0,(byte)0x34,
      (byte)0xf5,(byte)0x16,(byte)0xd1,(byte)0xbe,(byte)0x3b,(byte)0x88,(byte)0xb6,(byte)0xdb,(byte)0x11,
      (byte)0x6d,(byte)0x3e,(byte)0x87,(byte)0x1f,(byte)0x37,(byte)0x9b,(byte)0xb0,(byte)0x38,(byte)0xdc,
      (byte)0x07,(byte)0x8f,(byte)0xc9,(byte)0x80,(byte)0x51,(byte)0x65,(byte)0x36,(byte)0xff,(byte)0x4c,
      (byte)0x9e,(byte)0x49,(byte)0xac,(byte)0x12,(byte)0xcc,(byte)0xe8,(byte)0x74,(byte)0x82,(byte)0x18,
      (byte)0xec,(byte)0xe6,(byte)0xae,(byte)0xb7,(byte)0x63,(byte)0x89,(byte)0x71,(byte)0x21,(byte)0x7a,
      (byte)0xf1,(byte)0x0c,(byte)0x7c,(byte)0x76,(byte)0x0b,(byte)0xfc,(byte)0xb6,(byte)0x6a,(byte)0x84,
      (byte)0x2f,(byte)0x58,(byte)0x10,(byte)0x69,(byte)0xa0,(byte)0x11,(byte)0xb6,(byte)0x9e,(byte)0x40,
      (byte)0xf8,(byte)0xde,(byte)0x0d,(byte)0xcc,(byte)0x1d,(byte)0x25,(byte)0x31,(byte)0x7c,(byte)0x68,
      (byte)0x9f,(byte)0xfb,(byte)0xb1,(byte)0x6c,(byte)0x8f,(byte)0x20,(byte)0x21,(byte)0x88,(byte)0xc1,
      (byte)0xf4,(byte)0x7c,(byte)0xad,(byte)0x19,(byte)0x8b,(byte)0xae,(byte)0xb1,(byte)0xf8,(byte)0x8f,
      (byte)0x21,(byte)0x07,(byte)0x0d,(byte)0xef,(byte)0x59,(byte)0x23,(byte)0x82,(byte)0x75,(byte)0xba,
      (byte)0x55,(byte)0xe1,(byte)0x4e,(byte)0x92,(byte)0x08,(byte)0x77,(byte)0x5d,(byte)0xc1,(byte)0xcb,
      (byte)0xbc,(byte)0x0c,(byte)0x0c,(byte)0x48,(byte)0x33,(byte)0xe8,(byte)0x84,(byte)0xe0,(byte)0x03,
      (byte)0x75,(byte)0x84,(byte)0x09,(byte)0x74,(byte)0x5d,(byte)0x45,(byte)0xb4,(byte)0xb3,(byte)0x19,
      (byte)0x3e,(byte)0x6b,(byte)0x25,(byte)0xbe,(byte)0x16,(byte)0x49,(byte)0x93,(byte)0x66,(byte)0xa1,
      (byte)0x92,(byte)0x04,(byte)0x6f,(byte)0xab,(byte)0x81,(byte)0xc7,(byte)0x52,(byte)0x85,(byte)0x87,
      (byte)0x44,(byte)0x3a,(byte)0x93,(byte)0x10,(byte)0x30,(byte)0xe5,(byte)0xda,(byte)0x60,(byte)0xe4,
      (byte)0x7e,(byte)0x17,(byte)0xa2,(byte)0x0e,(byte)0x0b,(byte)0x7c,(byte)0xa7,(byte)0x0d,(byte)0xf8,
      (byte)0xd3,(byte)0xf1,(byte)0x28,(byte)0x72,(byte)0xe0,(byte)0xa5,(byte)0x0a,(byte)0xe1,(byte)0x6f,
      (byte)0x6e,(byte)0x84,(byte)0x33,(byte)0x6f,(byte)0x47,(byte)0x30,(byte)0x21,(byte)0x98,(byte)0x24,
      (byte)0x8b,(byte)0x82,(byte)0xa1,(byte)0xce,(byte)0x56,(byte)0x84,(byte)0xeb,(byte)0x0d,(byte)0x08,
      (byte)0x9e,(byte)0x2a,(byte)0x5b,(byte)0x57,(byte)0x30,(byte)0x5f,(byte)0xaa,(byte)0x82,(byte)0xbf,
      (byte)0xa9,(byte)0x11,(byte)0xfd,(byte)0xe2,(byte)0x2d,(byte)0x2b,(byte)0x82,(byte)0x89,(byte)0x12,
      (byte)0x15,(byte)0xe3,(byte)0xb5,(byte)0xd6,(byte)0x20,(byte)0x64,(byte)0xa7,(byte)0xc1,(byte)0x1d,
      (byte)0x57,(byte)0xc7,(byte)0x1f,(byte)0x26,(byte)0x8d,(byte)0x32,(byte)0x0f,(byte)0xbe,(byte)0x5a,
      (byte)0x13,(byte)0x66,(byte)0x4d,(byte)0x46,(byte)0xf4,(byte)0x89,(byte)0xd2,(byte)0x56,(byte)0x4a,
      (byte)0x70,(byte)0x15,(byte)0xcb,(byte)0x69,(byte)0x46,(byte)0x26,(byte)0x42,(byte)0xb0,(byte)0xb3,
      (byte)0x0d,(byte)0x3e,(byte)0xad,(byte)0x0c,(byte)0xde,(byte)0x52,(byte)0xc9,(byte)0x1a,(byte)0x98,
      (byte)0x95,(byte)0x67,(byte)0x83,(byte)0x2d,(byte)0x90,(byte)0x20,(byte)0xd0,(byte)0x7e,(byte)0x09,
      (byte)0x43,(byte)0xe2,(byte)0x6d,(byte)0xe8,(byte)0xcd,(byte)0xda,(byte)0xb4,(byte)0xd2,(byte)0xc4,
      (byte)0xd7,(byte)0x45,(byte)0x52,(byte)0xc1,(byte)0x0b,(byte)0x0d,(byte)0xe1,(byte)0x9e,(byte)0xab,
      (byte)0xd0,(byte)0x20,(byte)0x70,(byte)0xab,(byte)0x35,(byte)0xde,(byte)0xb0,(byte)0x79,(byte)0x95,
      (byte)0xf8,(byte)0x17,(byte)0xa8,(byte)0xc8,(byte)0x05,(byte)0x2b,(byte)0x8b,(byte)0xc1,(byte)0x32,
      (byte)0x31,(byte)0xf8,(byte)0xb6,(byte)0x16,(byte)0x8c,(byte)0x17,(byte)0x4b,(byte)0x97,(byte)0x61,
      (byte)0x77,(byte)0xb7,(byte)0x68,(byte)0xa3,(byte)0x60,(byte)0xd5,(byte)0x20,(byte)0x8d,(byte)0x15,
      (byte)0xe4,(byte)0x10,(byte)0x23,(byte)0x8a,(byte)0x03,(byte)0xfc,(byte)0xf4,(byte)0x61,(byte)0x15,
      (byte)0x02,(byte)0xd7,(byte)0x5a,(byte)0xf1,(byte)0xbd,(byte)0x9e,(byte)0x86,(byte)0x87,(byte)0x54,
      (byte)0xe2,(byte)0xb3,(byte)0x5a,(byte)0x01,(byte)0x6f,(byte)0x9d,(byte)0x19,(byte)0xfc,(byte)0xe5,
      (byte)0x16,(byte)0x4c,(byte)0xa8,(byte)0xf3,(byte)0xd1,(byte)0x93,(byte)0x95,(byte)0xca,(byte)0xc7,
      (byte)0x60,(byte)0x22,(byte)0xe9,(byte)0x28,(byte)0x3f,(byte)0x95,(byte)0xef,(byte)0x27,(byte)0x9e,
      (byte)0x1c,(byte)0xdc,(byte)0xcb,(byte)0x8e,(byte)0x4a,(byte)0x76,(byte)0xe1,(byte)0x4b,(byte)0xb5,
      (byte)0x11,(byte)0xde,(byte)0x86,(byte)0xf3,(byte)0xf1,(byte)0x7c,(byte)0xaa,(byte)0x3a,(byte)0x86,
      (byte)0x47,(byte)0x39,(byte)0x5b,(byte)0x97,(byte)0x61,(byte)0xf6,(byte)0x77,(byte)0x38,(byte)0xe9,
      (byte)0x32,(byte)0x0d,(byte)0x4a,(byte)0x77,(byte)0x0b,(byte)0x07,(byte)0xc4,(byte)0xe9,(byte)0x66,
      (byte)0x27,(byte)0xb1,(byte)0x93,(byte)0x79,(byte)0x90,(byte)0xbf,(byte)0x9d,(byte)0xeb,(byte)0x17,
      (byte)0x6d,(byte)0xe6,(byte)0x7a,(byte)0x73,(byte)0xd3,(byte)0x98,(byte)0x9e,(byte)0xec,(byte)0x54,
      (byte)0x73,(byte)0x77,(byte)0xe6,(byte)0x06,(byte)0xe1,(byte)0x5f,(byte)0xb7,(byte)0xf1,(byte)0x5f,
      (byte)0xf3,(byte)0x13,(byte)0x1d,(byte)0xd2,(byte)0xce,(byte)0xb9,(byte)0x49,(byte)0x72,(byte)0x1b,
      (byte)0xfe,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x00,(byte)0x49,(byte)0x45,(byte)0x4e,(byte)0x44,
      (byte)0xae,(byte)0x42,(byte)0x60,(byte)0x82
    };

    final Image IMAGE_DIRECTORY;
    final Image IMAGE_FILE;
    final Image IMAGE_FOLDER_UP;
    final Image IMAGE_FOLDER_NEW;
    final Image IMAGE_DELETE;

    /** dialog data
     */
    class Data
    {
      boolean showHidden;
    }

    /** file comparator
    */
    class FileComparator<T extends File> implements Comparator<T>
    {
      // Note: enum in non-static inner classes are not possible in Java, thus use the old way...
      private final static int SORTMODE_NONE     = 0;
      private final static int SORTMODE_NAME     = 1;
      private final static int SORTMODE_TYPE     = 2;
      private final static int SORTMODE_MODIFIED = 3;
      private final static int SORTMODE_SIZE     = 4;

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
      public int compare(T file1, T file2)
      {
        int result;
        if     (file1.isDirectory())
        {
          if   (file2.isDirectory()) result = 0;
          else                       result = -1;
        }
        else
        {
          if   (file2.isDirectory()) result = 1;
          else                       result = 0;
        }
        int nextSortMode = sortMode;
        while ((result == 0) && (nextSortMode != SORTMODE_NONE))
        {
          switch (nextSortMode)
          {
            case SORTMODE_NONE:
              break;
            case SORTMODE_NAME:
              result = file1.getName().compareTo(file2.getName());
              nextSortMode = SORTMODE_MODIFIED;
              break;
            case SORTMODE_TYPE:
              if     (file1.isDirectory())
              {
                if   (file2.isDirectory()) result = 0;
                else                       result = 1;
              }
              else
              {
                if   (file2.isDirectory()) result = -1;
                else                       result = 0;
              }
              nextSortMode = SORTMODE_NAME;
              break;
            case SORTMODE_MODIFIED:
              if      (file1.lastModified() < file2.lastModified()) result = -1;
              else if (file1.lastModified() > file2.lastModified()) result =  1;
              else                                                  result =  0;
              nextSortMode = SORTMODE_SIZE;
              break;
            case SORTMODE_SIZE:
              if      (file1.length() < file2.length()) result = -1;
              else if (file1.length() > file2.length()) result =  1;
              else                                      result =  0;
              nextSortMode = SORTMODE_NONE;
              break;
          }
        }

        return result;
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
      private Cursor                 CURSOR_WAIT;

      private Shell                  dialog;
      private FileComparator<T>      fileComparator;
      private ListDirectory<T>       listDirectory;
      private ListDirectoryFilter<T> listDirectoryFilter;
      private SimpleDateFormat       simpleDateFormat;
      private Pattern                fileFilterPattern;
      private boolean                showHidden;
      private boolean                showFiles;

      /** create update file list
       * @param listDirectory list directory
       * @param fileComparator file comparator
       */
      public Updater(Shell dialog, FileComparator<T> fileComparator, ListDirectory<T> listDirectory, ListDirectoryFilter<T> listDirectoryFilter, boolean showFiles)
      {
        this.CURSOR_WAIT         = new Cursor(dialog.getDisplay(),SWT.CURSOR_WAIT);

        this.dialog              = dialog;
        this.fileComparator      = fileComparator;
        this.listDirectory       = listDirectory;
        this.listDirectoryFilter = listDirectoryFilter;
        this.simpleDateFormat    = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        this.fileFilterPattern   = null;
        this.showHidden          = false;
        this.showFiles           =  showFiles;
      }

      /** update shortcut list
       * @param widgetShortcutList shortcut list widget
       * @param shortcutList shortcuts
       */
      public void updateShortcutList(List widgetShortcutList, ArrayList<T> shortcutList)
      {
        if (!widgetShortcutList.isDisposed())
        {
          widgetShortcutList.removeAll();
          for (T file : shortcutList)
          {
            widgetShortcutList.add(file.getAbsolutePath());
          }
        }
      }

      /** update file list
       * @param table table widget
       * @param imageDirectory, imageFile directory/file image (Note: required due to initialize+final)
       * @param path path
       * @param selectName name to select or null
       */
      public void updateFileList(Table table, final Image imageDirectory, final Image imageFile, T path, String selectName)
      {
        if (!table.isDisposed())
        {
          {
            if (!dialog.isDisposed())
            {
              dialog.setCursor(CURSOR_WAIT);
            }
          }
          try
          {
            table.removeAll();

            if (listDirectory.open(path))
            {
              // update list
              T file;
              while ((file = listDirectory.getNext()) != null)
              {
                if (   (showHidden || !listDirectory.isHidden(file))
                    && (showFiles || listDirectory.isDirectory(file))
                    && ((fileFilterPattern == null) || listDirectory.isDirectory(file) || fileFilterPattern.matcher(file.getName()).matches())
                    && ((listDirectoryFilter == null) || listDirectoryFilter.isAccepted(file))
                   )
                {
                  // find insert index
                  int index = 0;
                  TableItem tableItems[] = table.getItems();
                  while (   (index < tableItems.length)
                         && (fileComparator.compare(file,(T)tableItems[index].getData()) > 0)
                        )
                  {
                    index++;
                  }

                  // insert item
                  TableItem tableItem = new TableItem(table,SWT.NONE,index);
                  tableItem.setData(file);
                  tableItem.setText(0,file.getName());
                  if (file.isFile())      tableItem.setImage(0,imageFile);
                  if (file.isDirectory()) tableItem.setImage(0,imageDirectory);
// TODO:
//                  else if file.isSymbolicLink()) tableItem.setText(0,imageLink);
                  if (file.isFile() || file.isDirectory())
                  {
                    tableItem.setText(1,simpleDateFormat.format(new Date(file.lastModified())));
                  }
                  else
                  {
                    tableItem.setText(1,"");
                  }
                  if (file.isFile())
                  {
                    tableItem.setText(2,Long.toString(file.length()));
                  }
                  else
                  {
                    tableItem.setText(2,"");
                  }
                }
              }
              listDirectory.close();

              // select name
              if (selectName != null)
              {
                int index = 0;
                for (TableItem tableItem : table.getItems())
                {
                  if (((File)tableItem.getData()).getName().equals(selectName))
                  {
                    table.select(index);
                    break;
                  }
                  index++;
                }
              }
            }
          }
          finally
          {
            if (!dialog.isDisposed())
            {
              dialog.setCursor((Cursor)null);
            }
          }
        }
      }

      /** update file list
       * @param table table widget
       * @param imageDirectory, imageFile directory/file image (Note: required due to initialize+final)
       * @param path path
       */
      public void updateFileList(Table table, final Image imageDirectory, final Image imageFile, T path)
      {
        updateFileList(table,imageDirectory,imageFile,path,(String)null);
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

    T           result;
    Pane        pane;
    int         row1,row2;
    Composite   composite;
    Label       label;
    Button      button;
    Menu        menu;
    MenuItem    menuItem;
    TableColumn tableColumn;

    if (!parentShell.isDisposed())
    {
      // init images
      try
      {
        ByteArrayInputStream inputStream;

        inputStream = new ByteArrayInputStream(IMAGE_DIRECTORY_DATA_ARRAY);
        IMAGE_DIRECTORY = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();

        inputStream = new ByteArrayInputStream(IMAGE_FILE_DATA_ARRAY);
        IMAGE_FILE = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();

        inputStream = new ByteArrayInputStream(IMAGE_FOLDER_UP_DATA_ARRAY);
        IMAGE_FOLDER_UP = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();

        inputStream = new ByteArrayInputStream(IMAGE_FOLDER_NEW_DATA_ARRAY);
        IMAGE_FOLDER_NEW = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();

        inputStream = new ByteArrayInputStream(IMAGE_DELETE_DATA_ARRAY);
        IMAGE_DELETE = new Image(parentShell.getDisplay(),new ImageData(inputStream));
        inputStream.close();
      }
      catch (Exception exception)
      {
        throw new Error(exception);
      }

      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,0.0,0.0},1.0));

      final FileComparator fileComparator = new FileComparator(FileComparator.SORTMODE_NAME);
      final Updater        updater        = new Updater(dialog,
                                                        fileComparator,
                                                        listDirectory,
                                                        listDirectoryFilter,
                                                        (type != FileDialogTypes.DIRECTORY)
                                                       );
      final ArrayList<T>   shortcutList   = new ArrayList<T>();

      final Text   widgetPath;
      final Button widgetFolderUp;
      final Button widgetFolderNew;
      final Button widgetDelete;
      final List   widgetShortcutList;
      final Table  widgetFileList;
      final Combo  widgetFilter;
      final Button widgetShowHidden;
      final Text   widgetName;
      final Button widgetDone;
      final Object dragFile[] = new Object[1];
      DragSource   dragSource;
      DropTarget   dropTarget;

      row1 = 0;

      // path
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0},2));
      composite.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.WE));
      {
        label = new Label(composite,SWT.NONE);
        label.setText(Dialogs.tr("Path")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetPath = new Text(composite,SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
        Widgets.layout(widgetPath,0,1,TableLayoutData.WE);

        widgetFolderUp = new Button(composite,SWT.PUSH);
        widgetFolderUp.setToolTipText(Dialogs.tr("Goto parent folder."));
        widgetFolderUp.setImage(IMAGE_FOLDER_UP);
        Widgets.layout(widgetFolderUp,0,2,TableLayoutData.E);

        widgetFolderNew = new Button(composite,SWT.PUSH);
        widgetFolderNew.setToolTipText(Dialogs.tr("Create new folder."));
        widgetFolderNew.setImage(IMAGE_FOLDER_NEW);
        Widgets.layout(widgetFolderNew,0,3,TableLayoutData.E);

        widgetDelete = new Button(composite,SWT.PUSH);
        widgetDelete.setToolTipText(Dialogs.tr("Delete file or directory."));
        widgetDelete.setImage(IMAGE_DELETE);
        Widgets.layout(widgetDelete,0,4,TableLayoutData.E);
      }
      row1++;

      // create pane
      pane = Widgets.newPane(dialog,2,SWT.VERTICAL);
      pane.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.NSWE));
      row1++;

      // shortcut list
      composite = pane.getComposite(0);
      composite.setLayout(new TableLayout(1.0,1.0,2));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        widgetShortcutList = new List(composite,SWT.BORDER|SWT.V_SCROLL);
        widgetShortcutList.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        menu = Widgets.newPopupMenu(dialog);
        {
          menuItem = Widgets.addMenuItem(menu,Dialogs.tr("Remove"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              int index = widgetShortcutList.getSelectionIndex();
              if (index >= 0)
              {
                listDirectory.removeShortcut(shortcutList.get(index));
                listDirectory.getShortcuts(shortcutList);
                updater.updateShortcutList(widgetShortcutList,shortcutList);
              }
            }
          });
        }
        widgetShortcutList.setMenu(menu);
      }

      // file list
      composite = pane.getComposite(1);
      composite.setLayout(new TableLayout(1.0,1.0,2));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        widgetFileList = new Table(composite,SWT.BORDER);
        widgetFileList.setHeaderVisible(true);
        widgetFileList.setLinesVisible(true);
        widgetFileList.setLayout(new TableLayout(new double[]{1.0,0.0,0.0,0.0},1.0));
        widgetFileList.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

        SelectionListener selectionListener = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn tableColumn = (TableColumn)selectionEvent.widget;

            if      (tableColumn == widgetFileList.getColumn(0)) fileComparator.setSortMode(FileComparator.SORTMODE_NAME    );
            else if (tableColumn == widgetFileList.getColumn(1)) fileComparator.setSortMode(FileComparator.SORTMODE_MODIFIED);
            else if (tableColumn == widgetFileList.getColumn(2)) fileComparator.setSortMode(FileComparator.SORTMODE_SIZE    );
            Widgets.sortTableColumn(widgetFileList,tableColumn,fileComparator);
          }
        };

        tableColumn = new TableColumn(widgetFileList,SWT.LEFT);
        tableColumn.setText(Dialogs.tr("Name"));
        tableColumn.setData(new TableLayoutData(0,0,TableLayoutData.WE,0,0,0,0,600,SWT.DEFAULT));
        tableColumn.setResizable(true);
        tableColumn.addSelectionListener(selectionListener);

        tableColumn = new TableColumn(widgetFileList,SWT.LEFT);
        tableColumn.setText(Dialogs.tr("Modified"));
        tableColumn.setData(new TableLayoutData(0,2,TableLayoutData.NONE));
        tableColumn.setWidth(160);
        tableColumn.setResizable(false);
        tableColumn.addSelectionListener(selectionListener);

        tableColumn = new TableColumn(widgetFileList,SWT.RIGHT);
        tableColumn.setText(Dialogs.tr("Size"));
        tableColumn.setData(new TableLayoutData(0,3,TableLayoutData.NONE));
        tableColumn.setWidth(80);
        tableColumn.setResizable(false);
        tableColumn.addSelectionListener(selectionListener);
      }

      if (   (fileExtensions != null)
          || ((flags & FILE_SHOW_HIDDEN) != 0)
          || ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.SAVE) || (type == FileDialogTypes.ENTRY))
         )
      {
        // filter, name
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0},2));
        composite.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.WE));
        {
          row2 = 0;

          if (   (fileExtensions != null)
              || ((flags & FILE_SHOW_HIDDEN) != 0)
             )
          {
            if (fileExtensions != null)
            {
              label = new Label(composite,SWT.NONE);
              label.setText(Dialogs.tr("Filter")+":");
              Widgets.layout(label,row2,0,TableLayoutData.W);

              widgetFilter = new Combo(composite,SWT.NONE);
              widgetFilter.setLayoutData(new TableLayoutData(row2,1,TableLayoutData.WE));
            }
            else
            {
              widgetFilter = null;
            }

            if ((flags & FILE_SHOW_HIDDEN) != 0)
            {
              widgetShowHidden = new Button(composite,SWT.CHECK);
              widgetShowHidden.setText(Dialogs.tr("show hidden"));
              widgetShowHidden.setLayoutData(new TableLayoutData(row2,2,TableLayoutData.E));
            }
            else
            {
              widgetShowHidden = null;
            }

            row2++;
          }
          else
          {
            widgetFilter     = null;
            widgetShowHidden = null;
          }

          if ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.SAVE) || (type == FileDialogTypes.ENTRY))
          {
            label = new Label(composite,SWT.NONE);
            label.setText(Dialogs.tr("Name")+":");
            Widgets.layout(label,row2,0,TableLayoutData.W);

            widgetName = new Text(composite,SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
            widgetName.setEnabled((type != FileDialogTypes.OPEN));
            Widgets.layout(widgetName,row2,1,TableLayoutData.WE,0,2);

            row2++;
          }
          else
          {
            widgetName = null;
          }
        }
        row1++;
      }
      else
      {
        widgetFilter     = null;
        widgetShowHidden = null;
        widgetName       = null;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(row1,0,TableLayoutData.WE,0,0,2));
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
          case ENTRY:
            widgetDone.setText(Dialogs.tr("Select"));
            break;
        }
        widgetDone.setEnabled(false);
        widgetDone.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));

        button = new Button(composite,SWT.CENTER);
        button.setText(Dialogs.tr("Cancel"));
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }
      row1++;

      // install listeners
      widgetPath.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          T file = listDirectory.newFileInstance(widgetPath.getText());
          widgetPath.setData(file);

          updater.updateFileList(widgetFileList,
                                 IMAGE_DIRECTORY,
                                 IMAGE_FILE,
                                 file,
                                 (widgetName != null) ? widgetName.getText() : null
                                );
          if      (widgetFilter != null)
          {
            Widgets.setFocus(widgetFilter);
          }
          else if (widgetName != null)
          {
            Widgets.setFocus(widgetName);
          }
          else
          {
            Widgets.setFocus(widgetDone);
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetPath.addFocusListener(new FocusListener()
      {
        @Override
        public void focusGained(FocusEvent focusEvent)
        {
        }
        @Override
        public void focusLost(FocusEvent focusEvent)
        {
          T file = listDirectory.newFileInstance(widgetPath.getText());
          widgetPath.setData(file);
        }
      });
      widgetFolderUp.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          T file = (T)widgetPath.getData();

          T parentFile = listDirectory.getParentFile(file);
          if (parentFile != null)
          {
            // set new path, clear name
            widgetPath.setData(parentFile);
            widgetPath.setText(parentFile.getAbsolutePath());
            if ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.ENTRY)) widgetName.setText("");

            // update list
            updater.updateFileList(widgetFileList,
                                   IMAGE_DIRECTORY,
                                   IMAGE_FILE,
                                   parentFile,
                                   (widgetName != null) ? widgetName.getText() : null
                                  );

            // enable/disable done button
            widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                                  || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                                 );
          }
        }
      });
      widgetFolderNew.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          T file = (T)widgetPath.getData();

          String newSubDirectory = Dialogs.string(parentShell,
                                                  Dialogs.tr("New folder"),
                                                  Dialogs.tr("Path")+":",
                                                  "",
                                                  Dialogs.tr("Create")
                                                 );
          if (newSubDirectory != null)
          {
            try
            {
              T newDirectory = listDirectory.newFileInstance(widgetPath.getText(),newSubDirectory);

              listDirectory.mkdir(newDirectory);

              updater.updateFileList(widgetFileList,
                                     IMAGE_DIRECTORY,
                                     IMAGE_FILE,
                                     file,
                                     newSubDirectory
                                    );
            }
            catch (Exception exception)
            {
              Dialogs.error(parentShell,
                            Dialogs.tr("Cannot create new folder\n\n''{0}''\n\n(error: {1})!",
                                       newSubDirectory,
                                       exception.getMessage()
                                      )
                           );
            }
          }
        }
      });
      widgetDelete.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          T   file  = (T)widgetPath.getData();
          int index = widgetFileList.getSelectionIndex();

          T deleteFile;
          if (index >= 0)
          {
            TableItem tableItem    = widgetFileList.getItem(index);
            T         selectedFile = (T)tableItem.getData();

            deleteFile = listDirectory.newFileInstance(file.getAbsolutePath(),selectedFile.getName());
          }
          else
          {
            deleteFile = file;
          }

          if (Dialogs.confirm(parentShell,
                              Dialogs.tr(deleteFile.isFile() ? "Delete file ''{0}''?" : "Delete directory ''{0}''?",
                                         deleteFile.getAbsolutePath()
                                        ),
                              false
                             )
             )
          {
            try
            {
              listDirectory.delete(deleteFile);

              updater.updateFileList(widgetFileList,
                                     IMAGE_DIRECTORY,
                                     IMAGE_FILE,
                                     file
                                    );
            }
            catch (Exception exception)
            {
              Dialogs.error(parentShell,
                            Dialogs.tr("Cannot delete file\n\n''{0}''\n\n(error: {1})!",
                                       deleteFile.getAbsolutePath(),
                                       exception.getMessage()
                                      )
                           );
            }
          }
        }
      });
      widgetShortcutList.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          int index = widgetShortcutList.getSelectionIndex();
          if (index >= 0)
          {
            T file = shortcutList.get(index);

            if      (listDirectory.isDirectory(file))
            {
              // set new path, clear name
              widgetPath.setData(file);
              widgetPath.setText(file.getAbsolutePath());
              if ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.ENTRY)) widgetName.setText("");

              // update list
              updater.updateFileList(widgetFileList,
                                     IMAGE_DIRECTORY,
                                     IMAGE_FILE,
                                     (T)widgetPath.getData(),
                                     (widgetName != null) ? widgetName.getText() : null
                                    );
            }
            else if (listDirectory.exists(file))
            {
              error(dialog,Dialogs.tr("''{0}'' is not a directory!",file.getAbsolutePath()));
            }
            else
            {
              error(dialog,Dialogs.tr("''{0}'' does not exists",file.getAbsolutePath()));
            }

            // enable/disable done button
            widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                                  || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                                 );
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetShortcutList.addKeyListener(new KeyListener()
      {
        @Override
        public void keyPressed(KeyEvent keyEvent)
        {
        }
        @Override
        public void keyReleased(KeyEvent keyEvent)
        {
          if      (Widgets.isAccelerator(keyEvent,SWT.DEL))
          {
            int index = widgetShortcutList.getSelectionIndex();
            if (index >= 0)
            {
              listDirectory.removeShortcut(shortcutList.get(index));
              listDirectory.getShortcuts(shortcutList);
              updater.updateShortcutList(widgetShortcutList,shortcutList);
            }
          }
        }
      });
      widgetFileList.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          int index = widgetFileList.getSelectionIndex();
          if (index >= 0)
          {
            TableItem tableItem = widgetFileList.getItem(index);
            T         file      = (T)tableItem.getData();

            if (file.isDirectory())
            {
              // set new path, clear name
              T newPath = listDirectory.newFileInstance(((T)widgetPath.getData()).getAbsolutePath(),file.getName());
              widgetPath.setData(newPath);
              widgetPath.setText(newPath.getAbsolutePath());
              if ((type == FileDialogTypes.OPEN) || (type == FileDialogTypes.ENTRY)) widgetName.setText("");

              // update list
              updater.updateFileList(widgetFileList,
                                     IMAGE_DIRECTORY,
                                     IMAGE_FILE,
                                     (T)widgetPath.getData(),
                                     (widgetName != null) ? widgetName.getText() : null
                                    );
            }

            // enable/disable done button
            widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                                  || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                                 );
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          int index = widgetFileList.getSelectionIndex();
          if (index >= 0)
          {
            TableItem tableItem = widgetFileList.getItem(index);
            T         file      = (T)tableItem.getData();

            // set new name
            if (listDirectory.isFile(file))
            {
              if (widgetName != null) widgetName.setText(file.getName());
            }

            // enable/disable done button
            widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                                  || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                                 );
          }
        }
      });
      widgetFileList.addMouseListener(new MouseListener()
      {
        @Override
        public void mouseDoubleClick(MouseEvent mouseEvent)
        {
          TableItem[] tableItems = widgetFileList.getSelection();
          if (tableItems.length > 0)
          {
            T file = (T)tableItems[0].getData();
            if (!file.isDirectory())
            {
              // set new name
              if (widgetName != null) widgetName.setText(file.getName());

              // done
              fileGeometry = dialog.getSize();
              close(dialog,
                    (widgetName != null)
                      ? listDirectory.newFileInstance(widgetPath.getText(),widgetName.getText())
                      : listDirectory.newFileInstance(widgetPath.getText())
                   );
            }
          }
        }
        @Override
        public void mouseDown(MouseEvent mouseEvent)
        {
        }
        @Override
        public void mouseUp(MouseEvent mouseEvent)
        {
        }
      });
      if (widgetFilter != null)
      {
        widgetFilter.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Combo  widget = (Combo)selectionEvent.widget;
            String filter = widget.getText();
            if (filter.isEmpty()) filter = "*";

            widgetFilter.setText(filter);

            updater.setFileFilter(widgetFilter.getText());
            updater.updateFileList(widgetFileList,
                                   IMAGE_DIRECTORY,
                                   IMAGE_FILE,
                                   (T)widgetPath.getData(),
                                   (widgetName != null) ? widgetName.getText() : null
                                  );
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo  widget = (Combo)selectionEvent.widget;
            int index = widget.getSelectionIndex();
            if (index >= 0)
            {
              String filter = fileExtensions[index*2+1];

              widgetFilter.setText(filter);

              updater.setFileFilter(widgetFilter.getText());
              updater.updateFileList(widgetFileList,
                                     IMAGE_DIRECTORY,
                                     IMAGE_FILE,
                                     (T)widgetPath.getData(),
                                     (widgetName != null) ? widgetName.getText() : null
                                    );
            }
          }
        });
      }
      if ((flags & FILE_SHOW_HIDDEN) != 0)
      {
        widgetShowHidden.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            updater.setShowHidden(widgetShowHidden.getSelection());
            updater.updateFileList(widgetFileList,
                                   IMAGE_DIRECTORY,
                                   IMAGE_FILE,
                                   (T)widgetPath.getData(),
                                   (widgetName != null) ? widgetName.getText() : null
                                  );
          }
        });
      }
      if (widgetName != null)
      {
        widgetName.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            // enable/disable done button
            widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                                  || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                                  || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                                 );
          }
        });
        widgetName.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Widgets.setFocus(widgetDone);
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }
      widgetDone.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          fileGeometry = dialog.getSize();
          T file = (T)widgetPath.getData();
          close(dialog,
                (widgetName != null)
                  ? listDirectory.newFileInstance(file.getAbsolutePath(),widgetName.getText())
                  : file
               );
        }
      });

      // drag+drop
      dragSource = new DragSource(widgetFileList,DND.DROP_MOVE);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        @Override
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);
          TableItem tableItem = widgetFileList.getItem(point);
          if ((tableItem != null) && listDirectory.isDirectory((T)tableItem.getData()))
          {
            dragFile[0] = tableItem.getData();
          }
          else
          {
            dragSourceEvent.doit = false;
          }
        }
        @Override
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Point point         = new Point(dragSourceEvent.x,dragSourceEvent.y);
          TableItem tableItem = widgetFileList.getItem(point);
          if (tableItem != null)
          {
            dragSourceEvent.data = ((T)dragFile[0]).getAbsolutePath();
          }
        }
        @Override
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
          dragFile[0] = null;
        }
      });
      dropTarget = new DropTarget(widgetShortcutList,DND.DROP_MOVE|DND.DROP_COPY);
      dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dropTarget.addDropListener(new DropTargetAdapter()
      {
        @Override
        public void dragLeave(DropTargetEvent dropTargetEvent)
        {
        }
        @Override
        public void dragOver(DropTargetEvent dropTargetEvent)
        {
        }
        @Override
        public void drop(DropTargetEvent dropTargetEvent)
        {
          if (dropTargetEvent.data != null)
          {
            listDirectory.addShortcut((String)dropTargetEvent.data);
            listDirectory.getShortcuts(shortcutList);
            updater.updateShortcutList(widgetShortcutList,shortcutList);
          }
          else
          {
            dropTargetEvent.detail = DND.DROP_NONE;
          }
        }
      });

      // show
      show(dialog,fileGeometry);
      pane.setSizes(new double[]{0.25,0.75});

      // update shortcuts
      listDirectory.getShortcuts(shortcutList);
      updater.updateShortcutList(widgetShortcutList,shortcutList);

      // update path, name
      if (oldFile == null)
      {
        oldFile = (T)lastFile;
      }
      if (oldFile == null)
      {
        oldFile = listDirectory.getDefaultRoot();
      }
      if (oldFile != null)
      {
        if (widgetName != null)
        {
          if (oldFile.isDirectory())
          {
            if (!widgetPath.isDisposed())
            {
              widgetPath.setData(oldFile);
              widgetPath.setText(oldFile.getAbsolutePath());
            }
            if (!widgetName.isDisposed())
            {
              widgetName.setText("");
            }
            if (!widgetDone.isDisposed())
            {
              widgetDone.setEnabled(false);
            }
          }
          else
          {
            T parentFile = listDirectory.getParentFile(oldFile);
            widgetPath.setData(parentFile);
            if (parentFile != null)
            {
              if (!widgetPath.isDisposed())
              {
                widgetPath.setText(parentFile.getAbsolutePath());
              }
            }
            if (!widgetName.isDisposed())
            {
              widgetName.setText(oldFile.getName());
            }
            if (!widgetDone.isDisposed())
            {
              widgetDone.setEnabled(true);
            }
          }
        }
        else
        {
          if (!widgetPath.isDisposed())
          {
            widgetPath.setData(oldFile);
            widgetPath.setText(oldFile.getAbsolutePath());
          }
          if (!widgetDone.isDisposed())
          {
            widgetDone.setEnabled(true);
          }
        }
      }

      if ((fileExtensions != null) && (widgetFilter != null))
      {
        // update file extensions
        if (!widgetFilter.isDisposed())
        {
          for (int i = 0; i < fileExtensions.length; i+= 2)
          {
            widgetFilter.add(fileExtensions[i+0]+" ("+fileExtensions[i+1]+")");
          }
          widgetFilter.setText(defaultFileExtension);
        }
        updater.setFileFilter(defaultFileExtension);
       }

      // update file list
      if (   !widgetPath.isDisposed()
          && !widgetPath.getText().isEmpty()
         )
      {
        updater.updateFileList(widgetFileList,
                               IMAGE_DIRECTORY,
                               IMAGE_FILE,
                               (T)widgetPath.getData(),
                               (widgetName != null) ? widgetName.getText() : null
                              );
      }

      // run
      if (!widgetDone.isDisposed())
      {
        widgetDone.setEnabled(   ((type == FileDialogTypes.OPEN     ) && !widgetName.getText().isEmpty())
                              || ((type == FileDialogTypes.SAVE     ) && !widgetName.getText().isEmpty())
                              || ((type == FileDialogTypes.DIRECTORY) && !widgetPath.getText().isEmpty())
                              || ((type == FileDialogTypes.ENTRY    ) && (!widgetPath.getText().isEmpty() || !widgetName.getText().isEmpty()))
                             );
      }
      if (widgetName != null)
      {
        if (!widgetName.isDisposed())
        {
          widgetName.setFocus();
          widgetName.setSelection(new Point(0,widgetName.getText().length()));
        }
      }
      else
      {
        if (!widgetPath.isDisposed())
        {
          widgetPath.setFocus();
          widgetPath.setSelection(new Point(0,widgetPath.getText().length()));
        }
      }
      result = (T)run(dialog,null);
    }
    else
    {
      result = null;
    }

    // save last selected file
    if (result != null) lastFile = result;

    return (result != null) ? listDirectory.getAbsolutePath(result) : null;
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(final Shell            parentShell,
                                             final FileDialogTypes  type,
                                             String                 title,
                                             T                      oldFile,
                                             final String[]         fileExtensions,
                                             String                 defaultFileExtension,
                                             int                    flags,
                                             final ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFile,fileExtensions,defaultFileExtension,flags,listDirectory,(ListDirectoryFilter<T>)null);
  }
//file(Shell,FileDialogTypes,String,String,String[],String,int,ListDirectory<? extends File>,ListDirectoryFilter<File>)

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  public static <T extends File> String file(Shell                  parentShell,
                                             FileDialogTypes        type,
                                             String                 title,
                                             String                 oldFileName,
                                             String[]               fileExtensions,
                                             String                 defaultFileExtension,
                                             int                    flags,
                                             ListDirectory<T>       listDirectory,
                                             ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    T oldFile = !oldFileName.isEmpty()
                  ? listDirectory.newFileInstance(oldFileName)
                  : listDirectory.getDefaultRoot();
    return file(parentShell,type,title,oldFile,fileExtensions,defaultFileExtension,flags,listDirectory,listDirectoryFilter);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param fileExtensions array with {name,pattern} or null
   * @param defaultFileExtension default file extension pattern or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(Shell            parentShell,
                                             FileDialogTypes  type,
                                             String           title,
                                             String           oldFileName,
                                             String[]         fileExtensions,
                                             String           defaultFileExtension,
                                             int              flags,
                                             ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFileName,fileExtensions,defaultFileExtension,flags,listDirectory,(ListDirectoryFilter<T>)null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  public static <T extends File> String file(Shell                  parentShell,
                                             FileDialogTypes        type,
                                             String                 title,
                                             T                      oldFile,
                                             int                    flags,
                                             ListDirectory<T>       listDirectory,
                                             ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    return file(parentShell,type,title,oldFile,(String[])null,(String)null,flags,listDirectory);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(Shell            parentShell,
                                             FileDialogTypes  type,
                                             String           title,
                                             T                oldFile,
                                             int              flags,
                                             ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFile,(String[])null,(String)null,flags,listDirectory,(ListDirectoryFilter<T>)null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  public static <T extends File> String file(Shell                  parentShell,
                                             FileDialogTypes        type,
                                             String                 title,
                                             String                 oldFileName,
                                             int                    flags,
                                             ListDirectory<T>       listDirectory,
                                             ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    T oldFile = !oldFileName.isEmpty()
                  ? listDirectory.newFileInstance(oldFileName)
                  : (T)null;
    return file(parentShell,type,title,oldFile,flags,listDirectory,listDirectoryFilter);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param flags flags; see FILE_...
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(Shell            parentShell,
                                             FileDialogTypes  type,
                                             String           title,
                                             String           oldFileName,
                                             int              flags,
                                             ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFileName,flags,listDirectory,(ListDirectoryFilter<T>)null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  public static <T extends File> String file(Shell                  parentShell,
                                             FileDialogTypes        type,
                                             String                 title,
                                             T                      oldFile,
                                             ListDirectory<T>       listDirectory,
                                             ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    return file(parentShell,type,title,oldFile,FILE_NONE,listDirectory,listDirectoryFilter);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFile old file or null
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(Shell            parentShell,
                                             FileDialogTypes  type,
                                             String           title,
                                             T                oldFile,
                                             ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFile,FILE_NONE,listDirectory,(ListDirectoryFilter<T>)null);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param listDirectory list directory handler
   * @param listDirectoryFilter list directory filter or null
   * @return file name or null
   */
  public static <T extends File> String file(Shell                  parentShell,
                                             FileDialogTypes        type,
                                             String                 title,
                                             String                 oldFileName,
                                             ListDirectory<T>       listDirectory,
                                             ListDirectoryFilter<T> listDirectoryFilter
                                            )
  {
    T oldFile = !oldFileName.isEmpty()
                  ? listDirectory.newFileInstance(oldFileName)
                  : (T)null;
    return file(parentShell,type,title,oldFile,listDirectory,listDirectoryFilter);
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type file dialog type
   * @param title title text
   * @param oldFileName old file name or null
   * @param listDirectory list directory handler
   * @return file name or null
   */
  public static <T extends File> String file(Shell            parentShell,
                                             FileDialogTypes  type,
                                             String           title,
                                             String           oldFileName,
                                             ListDirectory<T> listDirectory
                                            )
  {
    return file(parentShell,type,title,oldFileName,listDirectory,(ListDirectoryFilter<T>)null);
  }

//TODO: replace by file() above
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
      for (int i = 0; i < fileExtensions.length/2; i++)
      {
        fileExtensionNames[i] = fileExtensions[i*2+0]+" ("+fileExtensions[i*2+1]+")";
      }
      String[] fileExtensionPatterns = new String[(fileExtensions.length+1)/2];
      int fileExtensionIndex = 0;
      for (int i = 0; i < fileExtensions.length/2; i++)
      {
        fileExtensionPatterns[i] = fileExtensions[i*2+1];
        if ((defaultFileExtension != null) && defaultFileExtension.equalsIgnoreCase(fileExtensions[i*2+1])) fileExtensionIndex = i;
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
//TODO: rename to entry() and use FileDialogTypes
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

    String    result;
    int       row;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
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
        widgetPath = new Text(composite,SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
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
        button.setLayoutData(new TableLayoutData(0,column,TableLayoutData.E));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetPath.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetPath.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      // run
      widgetPath.setFocus();
      result = (String)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
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

  /** string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list runnable for values
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String string(Shell        parentShell,
                              String       title,
                              String       text,
                              ListRunnable listRunnable,
                              String       okText,
                              String       cancelText,
                              String       toolTipText
                             )
  {
    String    result;
    int       row;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // string
      final Combo  widgetString;
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
        widgetString = new Combo(composite,SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
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
        widgetOkButton.setEnabled(false);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetString.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetString.addKeyListener(new KeyListener()
      {
        @Override
        public void keyPressed(KeyEvent keyEvent)
        {
        }
        @Override
        public void keyReleased(KeyEvent keyEvent)
        {
          Text widget = (Text)keyEvent.widget;

          widgetOkButton.setEnabled(!widget.getText().isEmpty());
        }
      });
      widgetString.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          if (!widget.getText().isEmpty())
          {
            widgetOkButton.setEnabled(true);
            widgetOkButton.setFocus();
          }
          else
          {
            widgetOkButton.setEnabled(false);
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      // show
      show(dialog);

      // fill-in values
      Collection<String> values;
      {
        dialog.setCursor(new Cursor(dialog.getDisplay(),SWT.CURSOR_WAIT));
      }
      try
      {
        values = listRunnable.getValues();
        if (values != null)
        {
          for (String value : values)
          {
            widgetString.add(value);
          }
        }
        String selectedValue = listRunnable.getSelection();
        if (selectedValue != null)
        {
          widgetString.setText(selectedValue);
        }
      }
      finally
      {
        dialog.setCursor((Cursor)null);
      }

      // run
      widgetString.setFocus();
      result = (String)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
  }

  /** string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list runnable for values
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return string or null on cancel
   */
  public static String string(Shell        parentShell,
                              String       title,
                              String       text,
                              ListRunnable listRunnable,
                              String       okText,
                              String       cancelText
                             )
  {
    return string(parentShell,title,text,listRunnable,okText,cancelText,(String)null);
  }

  /** string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list runnable for values
   * @param okText OK button text
   * @return string or null on cancel
   */
  public static String string(Shell        parentShell,
                              String       title,
                              String       text,
                              ListRunnable listRunnable,
                              String       okText
                            )
  {
    return string(parentShell,title,text,listRunnable,okText,Dialogs.tr("Cancel"));
  }

  /** string dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param listRunnable list runnable for values
   * @return string or null on cancel
   */
  public static String string(Shell        parentShell,
                              String       title,
                              String       text,
                              ListRunnable listRunnable
                            )
  {
    return string(parentShell,title,text,listRunnable,Dialogs.tr("Save"));
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
    String    result;
    int       row;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
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
        widgetString = new Text(composite,SWT.BORDER|SWT.SEARCH|SWT.ICON_CANCEL);
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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetString.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetString.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      // run
      widgetString.setFocus();
      result = (String)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
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
    return string(parentShell,title,text,value,okText,cancelText,(String)null);
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

  /** simple text dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return string or null on cancel
   */
  public static String[] text(Shell    parentShell,
                              String   title,
                              String   text,
                              String[] value,
                              String   okText,
                              String   cancelText,
                              String   toolTipText
                             )
  {
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Shell dialog = openModal(parentShell,title);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // text
      final StyledText widgetText;
      final Button     widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(new double[]{(text != null)?0.0:1.0,1.0},1.0,4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
      {
        int row = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(row,0,TableLayoutData.W));
          row++;
        }
        widgetText = new StyledText(composite,SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        if (value != null)
        {
          widgetText.setText(StringUtils.join(value,"\n"));
        }
        widgetText.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,300,200));
        if (toolTipText != null) widgetText.setToolTipText(toolTipText);
        row++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,widgetText.getText());
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetText.addSelectionListener(new SelectionListener()
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

      widgetText.setFocus();
      String result = (String)run(dialog,null);
      return (result != null) ? StringUtils.splitArray((String)run(dialog,null),'\n') : null;
    }
    else
    {
      return null;
    }
  }

  /** simple text dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return string or null on cancel
   */
  public static String[] text(Shell    parentShell,
                              String   title,
                              String   text,
                              String[] value,
                              String   okText,
                              String   cancelText
                             )
  {
    return text(parentShell,title,text,value,okText,cancelText,(String)null);
  }

  /** simple text dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @param okText OK button text
   * @return string or null on cancel
   */
  public static String[] text(Shell    parentShell,
                              String   title,
                              String   text,
                              String[] value,
                              String   okText
                             )
  {
    return text(parentShell,title,text,value,okText,Dialogs.tr("Cancel"));
  }


  /** simple text dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be null)
   * @return string or null on cancel
   */
  public static String[] text(Shell    parentShell,
                              String   title,
                              String   text,
                              String[] value
                             )
  {
    return text(parentShell,title,text,value,Dialogs.tr("Save"));
  }

  /** simple text dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @return string or null on cancel
   */
  public static String[] text(Shell  parentShell,
                              String title,
                              String text
                             )
  {
    return text(parentShell,title,text,new String[]{});
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
    Integer   result;
    int       row;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Shell dialog = openModal(parentShell,title,120,SWT.DEFAULT);
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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,new Integer(widgetInteger.getSelection()));
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetInteger.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetOkButton.setFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetInteger.setFocus();
      result = (Integer)run(dialog,null);
    }
    else
    {
      result =null;
    }

    return result;
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
    Integer   result;
    int       row;
    Composite composite,subComposite;
    Label     label;
    Button    button;

    if (increment < 1) throw new IllegalArgumentException();

    if (!parentShell.isDisposed())
    {
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
          widgetSlider.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));//,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
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
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            int newValue = (widgetSlider.getSelection() / increment) * increment;
            close(dialog,new Integer(newValue));
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,new Integer(value));
          }
        });
      }

      // install handlers
      widgetValue.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetOkButton.setFocus();
        }
        @Override
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

      // run
      widgetSlider.setFocus();
      result = (Integer)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
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
    String    result;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      final Cursor CURSOR_WAIT = new Cursor(dialog.getDisplay(),SWT.CURSOR_WAIT);

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
        widgetOkButton.setEnabled(false);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String selection[] = widgetList.getSelection();
            close(dialog,(selection.length > 0) ? selection[0] : (String)null);
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            close(dialog,null);
          }
        });
      }

      // install handlers
      widgetList.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          List widget = (List)selectionEvent.widget;

          widgetOkButton.setFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetList.addMouseListener(new MouseListener()
      {
        @Override
        public void mouseDoubleClick(MouseEvent mouseEvent)
        {
          List widget = (List)mouseEvent.widget;

          String selection[] = widgetList.getSelection();
          close(dialog,(selection.length > 0) ? selection[0] : (String)null);
        }
        @Override
        public void mouseDown(MouseEvent mouseEvent)
        {
        }
        public void mouseUp(MouseEvent mouseEvent)
        {
        }
      });

      // show
      show(dialog);

      // fill-in values
      Collection<String> values;
      {
        dialog.setCursor(CURSOR_WAIT);
      }
      try
      {
        values = listRunnable.getValues();
        if (values == null)
        {
          return null;
        }
        String selectedValue = listRunnable.getSelection();
        if (selectedValue == null)
        {
          return null;
        }
        int index = -1;
        int i     = 0;
        for (String value : values)
        {
          widgetList.add(value);
          if (selectedValue.equals(value)) index = i;
          i++;
        }
        if (index >= 0) widgetList.setSelection(index);
      }
      finally
      {
        dialog.setCursor((Cursor)null);
      }

      // run
      widgetList.setFocus();
      widgetOkButton.setEnabled(values.size() > 0);
      result = (String)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
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
                  public Collection<String> getValues()
                  {
                    return Arrays.asList(values);
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

  /** simple date/time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @param style widget style (SWT.CALENDAR, SWT.TIME)
   * @return date/time or null on cancel
   */
  public static Long dateTime(Shell  parentShell,
                              String title,
                              String text,
                              long   value,
                              String okText,
                              String cancelText,
                              String toolTipText,
                              int    style
                             )
  {
    Long      result;
    Composite composite;
    Label     label;
    Button    button;

    if (!parentShell.isDisposed())
    {
      final Shell dialog = openModal(parentShell,title,450,SWT.DEFAULT);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // string
      final DateTime widgetDate;
      final DateTime widgetTime;
      final Button   widgetOkButton;
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
      {
        int row    = 0;
        int column = 0;
        if (text != null)
        {
          label = new Label(composite,SWT.LEFT);
          label.setText(text);
          label.setLayoutData(new TableLayoutData(0,column,TableLayoutData.NW));
          column++;
        }
        if ((style & SWT.CALENDAR) != 0)
        {
          widgetDate = new DateTime(composite,SWT.CALENDAR);
          if (value != 0)
          {
            Calendar calendar = Calendar.getInstance();
            calendar.setTimeInMillis(value*1000);
            widgetDate.setDate(calendar.get(Calendar.YEAR),calendar.get(Calendar.MONTH),calendar.get(Calendar.DAY_OF_MONTH));
          }
          widgetDate.setLayoutData(new TableLayoutData(row,column,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
          if (toolTipText != null) widgetDate.setToolTipText(toolTipText);
          row++;
        }
        else
        {
          widgetDate = null;
        }
        if ((style & SWT.TIME) != 0)
        {
          widgetTime = new DateTime(composite,SWT.TIME);
          if (value != 0)
          {
            Calendar calendar = Calendar.getInstance();
            calendar.setTimeInMillis(value*1000);
            widgetTime.setTime(calendar.get(Calendar.HOUR_OF_DAY),calendar.get(Calendar.MONTH),calendar.get(Calendar.SECOND));
          }
          widgetTime.setLayoutData(new TableLayoutData(row,column,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
          if (toolTipText != null) widgetTime.setToolTipText(toolTipText);
          row++;
        }
        else
        {
          widgetTime = null;
        }
        column++;
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
      {
        widgetOkButton = new Button(composite,SWT.CENTER);
        widgetOkButton.setText(okText);
        widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
        widgetOkButton.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Calendar calendar = Calendar.getInstance();
            if (widgetDate != null)
            {
              calendar.set(Calendar.YEAR,widgetDate.getYear());
              calendar.set(Calendar.MONTH,widgetDate.getMonth());
              calendar.set(Calendar.DAY_OF_MONTH,widgetDate.getDay());
            }
            else
            {
              calendar.set(Calendar.YEAR,0);
              calendar.set(Calendar.MONTH,0);
              calendar.set(Calendar.DAY_OF_MONTH,1);
            }
            if (widgetTime != null)
            {
              calendar.set(Calendar.HOUR_OF_DAY,widgetDate.getHours());
              calendar.set(Calendar.MINUTE,widgetDate.getMinutes());
              calendar.set(Calendar.SECOND,widgetDate.getSeconds());
            }
            else
            {
              calendar.set(Calendar.HOUR,0);
              calendar.set(Calendar.MINUTE,0);
              calendar.set(Calendar.SECOND,0);
            }
            close(dialog,calendar.getTimeInMillis()/1000);
          }
        });

        button = new Button(composite,SWT.CENTER);
        button.setText(cancelText);
        button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT));
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
      widgetDate.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          widgetOkButton.setFocus();
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });

      // run
      widgetDate.setFocus();
      result = (Long)run(dialog,null);
    }
    else
    {
      result = null;
    }

    return result;
  }

  /** simple date/time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param style widget style (SWT.CALENDAR, SWT.TIME)
   * @return date [s] or null on cancel
   */
  public static Long dateTime(Shell  parentShell,
                              String title,
                              String text,
                              long   value,
                              String okText,
                              String cancelText,
                              int    style
                             )
  {
    return dateTime(parentShell,title,text,value,okText,cancelText,(String)null,style);
  }

  /** simple date/time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param style widget style (SWT.CALENDAR, SWT.TIME)
   * @return date [s] or null on cancel
   */
  public static Long dateTime(Shell  parentShell,
                              String title,
                              String text,
                              long   value,
                              String okText,
                              int    style
                             )
  {
    return dateTime(parentShell,title,text,value,okText,Dialogs.tr("Cancel"),style);
  }

  /** simple date/time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param okText OK button text
   * @param style widget style (SWT.CALENDAR, SWT.TIME)
   * @return date/time [s] or null on cancel
   */
  public static Long dateTime(Shell  parentShell,
                              String title,
                              String text,
                              String okText,
                              int    style
                             )
  {
    return dateTime(parentShell,title,text,0L,okText,style);
  }

  /** simple date/time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param okText OK button text
   * @param style widget style (SWT.CALENDAR, SWT.TIME)
   * @return date [s] or null on cancel
   */
  public static Long dateTime(Shell  parentShell,
                              String title,
                              String okText,
                              int    style
                             )
  {
    return dateTime(parentShell,title,(String)null,okText,style);
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return date/time [s] or null on cancel
   */
  public static Long date(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText,
                          String cancelText,
                          String toolTipText
                         )
  {
    return dateTime(parentShell,title,text,value,okText,cancelText,toolTipText,SWT.CALENDAR);
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return date [s] or null on cancel
   */
  public static Long date(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText,
                          String cancelText
                         )
  {
    return date(parentShell,title,text,value,okText,cancelText,(String)null);
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @return date [s] or null on cancel
   */
  public static Long date(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText
                         )
  {
    return date(parentShell,title,text,value,okText,Dialogs.tr("Cancel"));
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param okText OK button text
   * @return date [s] or null on cancel
   */
  public static Long date(Shell  parentShell,
                          String title,
                          String text,
                          String okText
                         )
  {
    return date(parentShell,title,text,0L,okText);
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param okText OK button text
   * @return date [s] or null on cancel
   */
  public static Long date(Shell  parentShell,
                          String title,
                          String okText
                         )
  {
    return date(parentShell,title,(String)null,okText);
  }

  /** simple time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @param toolTipText tooltip text (can be null)
   * @return time [s] or null on cancel
   */
  public static Long time(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText,
                          String cancelText,
                          String toolTipText
                         )
  {
    return dateTime(parentShell,title,text,value,okText,cancelText,toolTipText,SWT.TIME);
  }

  /** simple date dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @param cancelText cancel button text
   * @return time [s] or null on cancel
   */
  public static Long time(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText,
                          String cancelText
                         )
  {
    return time(parentShell,title,text,value,okText,cancelText,(String)null);
  }

  /** simple time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param value value to edit (can be 0)
   * @param okText OK button text
   * @return time [s] or null on cancel
   */
  public static Long time(Shell  parentShell,
                          String title,
                          String text,
                          long   value,
                          String okText
                         )
  {
    return time(parentShell,title,text,value,okText,Dialogs.tr("Cancel"));
  }

  /** simple time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text before input element
   * @param okText OK button text
   * @return time [s] or null on cancel
   */
  public static Long time(Shell  parentShell,
                          String title,
                          String text,
                          String okText
                         )
  {
    return time(parentShell,title,text,0L,okText);
  }

  /** simple time dialog
   * @param parentShell parent shell
   * @param title title string
   * @param okText OK button text
   * @return time [s] or null on cancel
   */
  public static Long time(Shell  parentShell,
                          String title,
                          String okText
                         )
  {
    return time(parentShell,title,(String)null,okText);
  }
}

/* end of file */
