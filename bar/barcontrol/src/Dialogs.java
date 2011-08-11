/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Dialogs.java,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.File;

import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
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
   * @param rowWeight row weight
   * @param columnWeight column weight
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double columnWeight)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    // create dialog
    final Shell dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
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
   * @param rowWeights row weights or null
   * @param columnWeight column weight
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double columnWeight)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    // create dialog
    final Shell dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
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
   * @param rowWeight row weight
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double rowWeight, double[] columnWeights)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    // create dialog
    final Shell dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
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
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    // create dialog
    final Shell dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
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
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight)
  {
    return open(parentShell,title,minWidth,minHeight,new double[]{1,0},1.0);
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

  /** show dialog
   * @param dialog dialog shell
   */
  static void show(Shell dialog)
  {
    int x,y;

    if (!dialog.isVisible())
    {
      // layout
      dialog.pack();

      // get location for dialog (keep 16/64 pixel away form right/bottom)
      Display display = dialog.getParent().getDisplay();
      Point cursorPoint = display.getCursorLocation();
      Rectangle displayBounds = display.getBounds();
      Rectangle bounds = dialog.getBounds();
      x = Math.min(Math.max(cursorPoint.x-bounds.width /2,0),displayBounds.width -bounds.width -16);
      y = Math.min(Math.max(cursorPoint.y-bounds.height/2,0),displayBounds.height-bounds.height-64);
      dialog.setLocation(x,y);

      // open dialog
      dialog.open();
    }
  }

  /** run dialog
   * @param dialog dialog shell
   * @param escapeKeyReturnValue value to return on ESC key
   */
  static Object run(final Shell dialog, final Object escapeKeyReturnValue)
  {
    Display display = dialog.getParent().getDisplay();

    final Object[] result = new Object[1];

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

    // close handler to get result
    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        // get result
        result[0] = dialog.getData();

        // set escape result if no result set
        if (result[0] == null) result[0] = escapeKeyReturnValue;

        // close the dialog
        dialog.dispose();
      }
    });

    // show
    show(dialog);

    // run dialog
    while (!dialog.isDisposed())
    {
      if (!display.readAndDispatch()) display.sleep();
    }

    return result[0];
  }

  /** run dialog
   * @param dialog dialog shell
   */
  static Object run(Shell dialog)
  {
    return run(dialog,null);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String title, Image image, String message)
  {
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell dialog = open(parentShell,title,200,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    // message
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
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
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

    run(dialog);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param title title
   * @param message info message
   */
  static void info(Shell parentShell, String title, String message)
  {
    Image image = Widgets.loadImage(parentShell.getDisplay(),"info.gif");

    info(parentShell,title,image,message);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String message)
  {
    info(parentShell,"Information",message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void error(Shell parentShell, String message)
  {
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell dialog = open(parentShell,"Error",200,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    Image image = Widgets.loadImage(parentShell.getDisplay(),"error.gif");

    // message
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

    run(dialog);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  static void error(Shell parentShell, String format, Object... arguments)
  {
    error(parentShell,String.format(format,arguments));
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void warning(Shell parentShell, String message)
  {
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell dialog = open(parentShell,"Warning",200,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    Image image = Widgets.loadImage(parentShell.getDisplay(),"warning.gif");

    // message
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

    run(dialog);
  }

  /** warning dialog
   * @param parentShell parent shell
   * @param format format string
   * @param arguments optional arguments
   */
  static void warning(Shell parentShell, String format, Object... arguments)
  {
    warning(parentShell,String.format(format,arguments));
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
  static boolean confirm(Shell parentShell, String title, Image image, String message, String yesText, String noText, boolean defaultValue)
  {
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final boolean[] result = new boolean[1];

    final Shell dialog = open(parentShell,title,200,70);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    // message
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
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText(yesText);
      if (defaultValue == true) button.setFocus();
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          close(dialog,true);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(noText);
      if (defaultValue == false) button.setFocus();
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    return (Boolean)run(dialog,false);
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
  static boolean confirm(Shell parentShell, String title, Image image, String message, String yesText, String noText)
  {
    return confirm(parentShell,title,image,message,yesText,noText,true);
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
  static boolean confirm(Shell parentShell, String title, String message, String yesText, String noText, boolean defaultValue)
  {
    Image image = Widgets.loadImage(parentShell.getDisplay(),"question.gif");

    return confirm(parentShell,title,image,message,yesText,noText,defaultValue);
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
    return confirm(parentShell,title,message,yesText,noText,true);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @return value
   */
  static boolean confirm(Shell parentShell, String title, String message, boolean defaultValue)
  {
    return confirm(parentShell,title,message,"Yes","No");
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

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  static boolean confirm(Shell parentShell, String message, boolean defaultValue)
  {
    return confirm(parentShell,"Confirm",message,"Yes","No",defaultValue);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param message confirmation message
   * @return value
   */
  static boolean confirm(Shell parentShell, String message)
  {
    return confirm(parentShell,"Confirm",message,"Yes","No",true);
  }

  /** confirmation error dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   * @return value
   */
  static boolean confirmError(Shell parentShell, String title, String message, String yesText, String noText)
  {
    Image image = Widgets.loadImage(parentShell.getDisplay(),"error.gif");

    return confirm(parentShell,title,image,message,yesText,noText);
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   * @param defaultValue default value
   * @return selection index (0..n-1)
   */
  static int select(Shell parentShell, String title, String message, String[] texts, String[] helpTexts, int defaultValue)
  {
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final int[] result = new int[1];

    final Shell dialog = open(parentShell,title);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

    Image image = Widgets.loadImage(parentShell.getDisplay(),"question.gif");

    // message
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
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
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
        button.setLayoutData(new TableLayoutData(0,n,TableLayoutData.WE,0,0,0,0,textWidth+20,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;

            close(dialog,widget.getData());
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        if ((helpTexts != null) && (n < helpTexts.length))
        {
          button.setToolTipText(helpTexts[n]);
        }

        n++;
      }
    }

    return (Integer)run(dialog,defaultValue);
  }

  static int select(Shell parentShell, String title, String message, String[] texts, int defaultValue)
  {
    return select(parentShell,title,message,texts,null,defaultValue);
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
  static String password(Shell parentShell, String title, String message, String text1, final String text2, String okText, String cancelText)
  {
    int             row;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final String[] result = new String[1];

    final Shell dialog = open(parentShell,title,450,SWT.DEFAULT);
    dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

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
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(row+0,0,TableLayoutData.WE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setText(text1);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

      widgetPassword1 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
      widgetPassword1.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE,0,0,0,0,200,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));

      if (text2 != null)
      {
        label = new Label(composite,SWT.LEFT);
        label.setText(text2);
        label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));

        widgetPassword2 = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
        widgetPassword2.setLayoutData(new TableLayoutData(1,1,TableLayoutData.WE,0,0,0,0,200,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
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
      widgetOkButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
      widgetOkButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          String password1 = widgetPassword1.getText();
          if (text2 != null)
          {
            String password2 = widgetPassword2.getText();
            if (password1.equals(password2))
            {
              close(dialog,password1);
            }
            else
            {
              Widgets.flash(widgetPassword2);
              widgetPassword2.setFocus();
            }
          }
          else
          {
            close(dialog,password1);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(cancelText);
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          close(dialog,null);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // install handlers
    widgetPassword1.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;

        if (text2 != null)
        {
          widgetPassword2.forceFocus();
        }
        else
        {
          widgetOkButton.forceFocus();
        }
      }
    });
    if (text2 != null)
    {
      widgetPassword2.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;

          widgetOkButton.forceFocus();
        }
      });
    }

    return (String)run(dialog,null);
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text1 text
   * @param text2 text (can be null)
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title, String message, String text1, String text2)
  {
    return password(parentShell,title,message,text1,text2,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title, String message, String text)
  {
    return password(parentShell,title,message,text,null,"OK","Cancel");
  }

  /** password dialog
   * @param parentShell parent shell
   * @param title title string
   * @param text text
   * @return password or null on cancel
   */
  static String password(Shell parentShell, String title, String text)
  {
    return password(parentShell,title,null,text,null);
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
