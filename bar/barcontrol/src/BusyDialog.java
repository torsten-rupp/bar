/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BusyDialog.java,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: busy dialog
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
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
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

/** busy dialog
 */
class BusyDialog
{
  private Display display;

  private Image   images[] = new Image[2];
  private long    imageTimestamp;
  private int     imageIndex;

  private Shell   dialog;
  private Label   widgetImage;
  private Label   widgetMessage;
  private Label   widgetText;
  private Button  widgetAbortButton;

  private boolean abortedFlag;
  private boolean resizedFlag;

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite,subComposite;
    Label           label;
    int             x,y;

    display = parentShell.getDisplay();

    // load images
    images[0] = Widgets.loadImage(parentShell.getDisplay(),"busy0.gif");
    images[1] = Widgets.loadImage(parentShell.getDisplay(),"busy1.gif");
    imageTimestamp = System.currentTimeMillis();
    imageIndex = 0;

    // create dialog
    dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    dialog.setText(title);
    tableLayout = new TableLayout(new double[]{1.0,0.0},1.0);
    tableLayout.minWidth = 300;
    dialog.setLayout(tableLayout);
    dialog.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
if (false)
{
      widgetImage = new Label(composite,SWT.LEFT|SWT.BORDER);
      widgetImage.setImage(images[imageIndex]);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,2,0,4,4));

      widgetMessage = new Label(composite,SWT.LEFT|SWT.BORDER);
      if (message != null) widgetMessage.setText(message);
      widgetMessage.setLayoutData(new TableLayoutData(0,1,TableLayoutData.N|TableLayoutData.WE,0,0,4,4));

      widgetText = new Label(composite,SWT.LEFT|SWT.BORDER);
      widgetText.setLayoutData(new TableLayoutData(1,1,TableLayoutData.S|TableLayoutData.W,0,0,4,4));
}
else
{
      widgetImage = new Label(composite,SWT.LEFT);
      widgetImage.setImage(images[imageIndex]);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,4));

      subComposite = new Composite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(new double[]{0.0,1.0},1.0));
      subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
      {
        if (message != null)
        {
          widgetMessage = new Label(subComposite,SWT.LEFT);
          widgetMessage.setText(message);
          widgetMessage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.N|TableLayoutData.WE));

          widgetText = new Label(subComposite,SWT.LEFT);
          widgetText.setLayoutData(new TableLayoutData(1,0,TableLayoutData.S|TableLayoutData.WE));
        }
        else
        {
          widgetMessage = null;

          widgetText = new Label(subComposite,SWT.LEFT);
          widgetText.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
        }
      }
}
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4,4));
    {
      widgetAbortButton = new Button(composite,SWT.CENTER|SWT.BORDER);
      widgetAbortButton.setText("Abort");
      widgetAbortButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE)); //,0,0,0,0,60,SWT.DEFAULT));
      widgetAbortButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          abort();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // resize handler
    dialog.addListener(SWT.Resize,new Listener()
    {
      public void handleEvent(Event event)
      {
        resizedFlag = true;
      }
    });

    // add escape key handler
    dialog.addTraverseListener(new TraverseListener()
    {
      public void keyTraversed(TraverseEvent traverseEvent)
      {
        Shell widget = (Shell)traverseEvent.widget;

        if (traverseEvent.detail == SWT.TRAVERSE_ESCAPE)
        {
          abort();
          traverseEvent.doit = false;
        }
      }
    });

    // close handler to abort
    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        abort();
        event.doit = false;
      }
    });

    // pack
    dialog.pack();

    // get location for dialog (keep 16 pixel away form right/bottom)
    Point cursorPoint = display.getCursorLocation();
    Rectangle displayBounds = display.getBounds();
    Rectangle bounds = dialog.getBounds();
    x = Math.min(Math.max(cursorPoint.x-bounds.width /2,0),displayBounds.width -bounds.width -16);
    y = Math.min(Math.max(cursorPoint.y-bounds.height/2,0),displayBounds.height-bounds.height-64);
    dialog.setLocation(x,y);

    // run dialog
    resizedFlag = false;
    abortedFlag = false;
    dialog.open();
  }

  /** close busy dialog
   */
  public void close()
  {
    display.syncExec(new Runnable()
    {
      public void run()
      {
        dialog.dispose();
      }
    });
  }

  /** close busy dialog
   */
  public void abort()
  {
    abortedFlag = true;
    widgetAbortButton.setEnabled(false);
  }

  /** check if "cancel" button clicked
   * @return true iff "cancel" button clicked, false otherwise
   */
  public boolean isAborted()
  {
    return abortedFlag;
  }

  public void setMessage(final String message)
  {
    if ((widgetMessage != null) && !dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetMessage.setText(message);
          display.update();

          /* resize dialog (it not manually changed) */
          if (!resizedFlag)
          {
            GC gc = new GC(widgetMessage);
            int width = gc.stringExtent(message).x;
            gc.dispose();

            if (widgetMessage.getSize().x < width) dialog.pack();
          }
        }
      });
    }
  }

  /** update busy dialog
   * @param text text to show
   * @return true if closed, false otherwise
   */
  public boolean update(final String text)
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          long timestamp = System.currentTimeMillis();
          if (timestamp > (imageTimestamp+250))
          {
            imageTimestamp = timestamp;
            imageIndex     = (imageIndex+1)%2;
            widgetImage.setImage(images[imageIndex]);
            display.update();
          }
          if (text != null)
          {
            widgetText.setText(text);
            display.update();

            /* resize dialog (it not manually changed) */
            if (!resizedFlag)
            {
              GC gc = new GC(widgetText);
              int width = gc.stringExtent(text).x;
              gc.dispose();

              if (widgetText.getSize().x < width) dialog.pack();
            }
          }
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog
   * @param n number to show
   */
  public boolean update(Long n)
  {
    return update(Long.toString(n));
  }

  /** update busy dialog
   */
  public boolean update()
  {
    return update((String)null);
  }
}

/* end of file */
