/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Dialogs.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.File;

import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

class Dialogs
{
  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights)
  {
    int             x,y;
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    // get location for dialog (keep 16 pixel away form right/bottom)
    Display display = parentShell.getDisplay();
    Point cursorPoint = display.getCursorLocation();
    Rectangle bounds = display.getBounds();
    x = Math.min(Math.max(cursorPoint.x-minWidth /2,0),bounds.width -minWidth -16);
    y = Math.min(Math.max(cursorPoint.y-minHeight/2,0),bounds.height-minHeight-16);

    // create dialog
    final Shell shell = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    shell.setText(title);
    shell.setLocation(x,y);
    tableLayout = new TableLayout(rowWeights,columnWeights,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    shell.setLayout(tableLayout);

    return shell;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight)
  {
    return open(parentShell,title,minWidth,minHeight,new double[]{1,0},null);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  static void close(Shell dialog, Object returnValue)
  {
    dialog.setData(returnValue);
    dialog.close();
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  static void close(Shell dialog)
  {
    close(dialog,null);
  }

  /** run dialog
   * @param dialog dialog shell
   */
  static Object run(Shell dialog)
  {
    final Object[] result = new Object[1];

    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        result[0] = widget.getData();
      }
    });

    dialog.pack();
    dialog.open();
    Display display = dialog.getParent().getDisplay();
    while (!dialog.isDisposed())
    {
      if (!display.readAndDispatch()) display.sleep();
    }

    return result[0];
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String title, Image image, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell shell = open(parentShell,title,200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.DEFAULT|TableLayoutData.EXPAND_X));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String title, String message)
  {
/*
    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);
    ImageData imageData = new ImageData(Images.info.width,Images.info.height,Images.info.depth,paletteData,1,Images.info.data);
    imageData.alphaData = Images.info.alphas;
    imageData.alpha = -1;
    Image image = new Image(parentShell.getDisplay(),imageData);
*/
    Image image = new Image(parentShell.getDisplay(),"images/info.gif");

    info(parentShell,title,image,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void error(Shell parentShell, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell shell = open(parentShell,"Error",200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/error.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.DEFAULT|TableLayoutData.EXPAND_X));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  static boolean confirm(Shell parentShell, String title, String message, String yesText, String noText)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final boolean[] result = new boolean[1];

    final Shell shell = open(parentShell,title,200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/question.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText(yesText);
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = true;
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(noText);
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = false;
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);

    return result[0];
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  static boolean confirm(Shell parentShell, String title, String message)
  {
    return confirm(parentShell,title,message,"Yes","No");
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @return selection index (0..n-1)
   */
  static int select(Shell parentShell, String title, String message, String[] texts)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final int[] result = new int[1];

//    final Shell shell = open(parentShell,title,200,SWT.DEFAULT);
    final Shell shell = open(parentShell,title);
//    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/question.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
//    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
//      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE|TableLayoutData.EXPAND));
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      int textWidth = 0;
      GC gc = new GC(composite);
      for (String text : texts)
      {
        textWidth = Math.max(textWidth,gc.textExtent(text).x);
      }
      gc.dispose();

      int n = 0;
      for (String text : texts)
      {
        button = new Button(composite,SWT.CENTER);
        button.setText(text);
        button.setData(n);
        button.setLayoutData(new TableLayoutData(0,n,TableLayoutData.EXPAND_X,0,0,0,0,textWidth,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;

            result[0] = ((Integer)widget.getData()).intValue();
            shell.close();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        n++;
      }
    }

    run(shell);

    return result[0];
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @param okText OK button text
   * @param CancelText cancel button text
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title, String text, String okText, String cancelText)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final String[] result = new String[1];

    final Shell shell = open(parentShell,title,250,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // password
    final Text widgetText;
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      label = new Label(composite,SWT.LEFT);
      label.setText(text);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

      widgetText = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
      widgetText.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText(okText);
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = widgetText.getText();
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(cancelText);
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = null;
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);

    return result[0];
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title, String text)
  {
    return password(parentShell,title,text,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title)
  {
    return password(parentShell,title,"Password:");
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type SWT.OPEN or SWT.SAVE
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  private static String file(Shell parentShell, int type, String title, String fileName, String[] fileExtensions)
  {
    FileDialog dialog = new FileDialog(parentShell,type);
    dialog.setText(title);
    if (fileName != null)
    {
      dialog.setFilterPath(new File(fileName).getParent());
      dialog.setFileName(new File(fileName).getName());
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
      for (int z = 0; z < fileExtensions.length/2; z++)
      {
        fileExtensionPatterns[z] = fileExtensions[z*2+1];
      }
      dialog.setFilterNames(fileExtensionNames);
      dialog.setFilterExtensions(fileExtensionPatterns);
    }

    return dialog.open();  
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  static String fileOpen(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return file(parentShell,SWT.OPEN,title,fileName,fileExtensions);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  static String fileSave(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return file(parentShell,SWT.SAVE,title,fileName,fileExtensions);
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @param pathName path name or null
   * @return directory name or null
   */
  static String directory(Shell parentShell, String title, String pathName)
  {
    DirectoryDialog dialog = new DirectoryDialog(parentShell);
    dialog.setText(title);
    if (pathName != null)
    {
      dialog.setFilterPath(pathName);
    }

    return dialog.open();  
  }
}

/* end of file */
