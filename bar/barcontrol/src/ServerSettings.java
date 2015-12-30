/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabJobs.java,v $
* $Revision: 1.29 $
* $Author: torsten $
* Contents: Server settings
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;

import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/**
 * Edit BAR server settings
 */
public class ServerSettings
{
  /**
   * Server types
   */
  enum ServerTypes
  {
    NONE,

    FILESYSTEM,
    FTP,
    SSH,
    WEBDAV;

    /** parse type string
     * @param string type string
     * @return priority
     */
    static ServerTypes parse(String string)
    {
      ServerTypes type;

      if      (string.equalsIgnoreCase("filesystem"))
      {
        type = ServerTypes.FILESYSTEM;
      }
      else if (string.equalsIgnoreCase("ftp"))
      {
        type = ServerTypes.FTP;
      }
      else if (string.equalsIgnoreCase("ssh"))
      {
        type = ServerTypes.SSH;
      }
      else if (string.equalsIgnoreCase("webdav"))
      {
        type = ServerTypes.WEBDAV;
      }
      else
      {
        type = ServerTypes.NONE;
      }

      return type;
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case FILESYSTEM: return "filesystem";
        case FTP:        return "ftp";
        case SSH:        return "ssh";
        case WEBDAV:     return "webdav";
        default:         return "";
      }
    }
  };

  /** server data
   */
  static class ServerData
  {
    int         id;
    String      name;
    ServerTypes type;

    /** create server data
     * @param name name
     * @param type server type
     */
    ServerData(int id, String name, ServerTypes type)
    {
      this.id   = id;
      this.name = name;
      this.type = type;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Server {"+id+", "+name+", "+type.toString()+"}";
    }
  };

  /** server data comparator
   */
  static class ServerDataComparator implements Comparator<ServerData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_TYPE = 0;
    private final static int SORTMODE_NAME = 1;

    private int sortMode;

    /** create server data comparator
     * @param table server table
     * @param sortColumn column to sort
     */
    ServerDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_NAME;
      else                                       sortMode = SORTMODE_TYPE;
    }

    /** create server data comparator
     * @param table server table
     */
    ServerDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare server data
     * @param serverData1, serverData2 server data to compare
     * @return -1 iff serverData1 < serverData2,
                0 iff serverData1 = serverData2,
                1 iff serverData1 > serverData2
     */
    public int compare(ServerData serverData1, ServerData serverData2)
    {
      switch (sortMode)
      {
        case SORTMODE_TYPE:
Dprintf.dprintf("-----------------------------------------------------------");
Dprintf.dprintf("%s %s",serverData1,serverData2);
          return serverData1.type.toString().compareTo(serverData2.type.toString());
        case SORTMODE_NAME:
          return serverData1.name.compareTo(serverData2.name);
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "ServerDataComparator {"+sortMode+"}";
    }
  }

  /** edit server settings
   * @param shell shell
   */
  public static void serverSettings(Shell shell)
  {
    TabFolder   tabFolder,subTabFolder;
    Composite   tab;
    Composite   composite,subComposite,subSubComposite;
    Label       label;
    Text        text;
    Combo       combo;
    Spinner     spinner;
    TableColumn tableColumn;
    Button      button;
    int         row;

    WidgetVariable          tmpDirectory               = new WidgetVariable<String>("",    "tmp-directory"                  );
    WidgetVariable          maxTmpSize                 = new WidgetVariable<String>("",    "max-tmp-size"                   );
    WidgetVariable          niceLevel                  = new WidgetVariable<Integer>(0,    "nice-level"                     );
    WidgetVariable          maxThreads                 = new WidgetVariable<Integer>(0,    "max-threads"                    );
    WidgetVariable          maxBandWidth               = new WidgetVariable<String>("",    "max-band-width"                 );
    WidgetVariable          compressMinSize            = new WidgetVariable<String>("",    "compress-min-size"              );
    WidgetVariable          serverJobsDirectory        = new WidgetVariable<String>("",    "server-jobs-directory"          );

    WidgetVariable          indexDatabase              = new WidgetVariable<String>("",    "index-database"                 );
    WidgetVariable          indexDatabaseAutoUpdate    = new WidgetVariable<Boolean>(false,"index-database-auto-update"     );
//    WidgetVariable          indexDatabaseMaxBandWidth  = new WidgetVariable<Integer>(0,"index-database-max-band-width"  );
    WidgetVariable          indexDatabaseKeepTime      = new WidgetVariable<String>("",    "index-database-keep-time"       );

    WidgetVariable          cdDevice                   = new WidgetVariable<String>("",    "cd-device"                      );
    WidgetVariable          cdRequestVolumeCommand     = new WidgetVariable<String>("",    "cd-request-volume-command"      );
    WidgetVariable          cdUnloadCommand            = new WidgetVariable<String>("",    "cd-unload-volume-command"       );
    WidgetVariable          cdLoadCommand              = new WidgetVariable<String>("",    "cd-load-volume-command"         );
    WidgetVariable          cdVolumeSize               = new WidgetVariable<String>("",    "cd-volume-size"                 );
    WidgetVariable          cdImagePreCommand          = new WidgetVariable<String>("",    "cd-image-pre-command"           );
    WidgetVariable          cdImagePostCommand         = new WidgetVariable<String>("",    "cd-image-post-command"          );
    WidgetVariable          cdImageCommandCommand      = new WidgetVariable<String>("",    "cd-image-command"               );
    WidgetVariable          cdECCPreCommand            = new WidgetVariable<String>("",    "cd-ecc-pre-command"             );
    WidgetVariable          cdECCPostCommand           = new WidgetVariable<String>("",    "cd-ecc-post-command"            );
    WidgetVariable          cdECCCommand               = new WidgetVariable<String>("",    "cd-ecc-command"                 );
    WidgetVariable          cdWritePreCommand          = new WidgetVariable<String>("",    "cd-write-pre-command"           );
    WidgetVariable          cdWritePostCommand         = new WidgetVariable<String>("",    "cd-write-post-command"          );
    WidgetVariable          cdWriteCommand             = new WidgetVariable<String>("",    "cd-write-command"               );
    WidgetVariable          cdWriteImageCommand        = new WidgetVariable<String>("",    "cd-write-image-command"         );

    WidgetVariable          dvdDevice                  = new WidgetVariable<String>("",    "dvd-device"                     );
    WidgetVariable          dvdRequestVolumeCommand    = new WidgetVariable<String>("",    "dvd-request-volume-command"     );
    WidgetVariable          dvdUnloadCommand           = new WidgetVariable<String>("",    "dvd-unload-volume-command"      );
    WidgetVariable          dvdLoadCommand             = new WidgetVariable<String>("",    "dvd-load-volume-command"        );
    WidgetVariable          dvdVolumeSize              = new WidgetVariable<String>("",    "dvd-volume-size"                );
    WidgetVariable          dvdImagePreCommand         = new WidgetVariable<String>("",    "dvd-image-pre-command"          );
    WidgetVariable          dvdImagePostCommand        = new WidgetVariable<String>("",    "dvd-image-post-command"         );
    WidgetVariable          dvdImageCommandCommand     = new WidgetVariable<String>("",    "dvd-image-command"              );
    WidgetVariable          dvdECCPreCommand           = new WidgetVariable<String>("",    "dvd-ecc-pre-command"            );
    WidgetVariable          dvdECCPostCommand          = new WidgetVariable<String>("",    "dvd-ecc-post-command"           );
    WidgetVariable          dvdECCCommand              = new WidgetVariable<String>("",    "dvd-ecc-command"                );
    WidgetVariable          dvdWritePreCommand         = new WidgetVariable<String>("",    "dvd-write-pre-command"          );
    WidgetVariable          dvdWritePostCommand        = new WidgetVariable<String>("",    "dvd-write-post-command"         );
    WidgetVariable          dvdWriteCommand            = new WidgetVariable<String>("",    "dvd-write-command"              );
    WidgetVariable          dvdWriteImageCommand       = new WidgetVariable<String>("",    "dvd-write-image-command"        );

    WidgetVariable          bdDevice                   = new WidgetVariable<String>("",    "bd-device"                      );
    WidgetVariable          bdRequestVolumeCommand     = new WidgetVariable<String>("",    "bd-request-volume-command"      );
    WidgetVariable          bdUnloadCommand            = new WidgetVariable<String>("",    "bd-unload-volume-command"       );
    WidgetVariable          bdLoadCommand              = new WidgetVariable<String>("",    "bd-load-volume-command"         );
    WidgetVariable          bdVolumeSize               = new WidgetVariable<String>("",    "bd-volume-size"                 );
    WidgetVariable          bdImagePreCommand          = new WidgetVariable<String>("",    "bd-image-pre-command"           );
    WidgetVariable          bdImagePostCommand         = new WidgetVariable<String>("",    "bd-image-post-command"          );
    WidgetVariable          bdImageCommandCommand      = new WidgetVariable<String>("",    "bd-image-command"               );
    WidgetVariable          bdECCPreCommand            = new WidgetVariable<String>("",    "bd-ecc-pre-command"             );
    WidgetVariable          bdECCPostCommand           = new WidgetVariable<String>("",    "bd-ecc-post-command"            );
    WidgetVariable          bdECCCommand               = new WidgetVariable<String>("",    "bd-ecc-command"                 );
    WidgetVariable          bdWritePreCommand          = new WidgetVariable<String>("",    "bd-write-pre-command"           );
    WidgetVariable          bdWritePostCommand         = new WidgetVariable<String>("",    "bd-write-post-command"          );
    WidgetVariable          bdWriteCommand             = new WidgetVariable<String>("",    "bd-write-command"               );
    WidgetVariable          bdWriteImageCommand        = new WidgetVariable<String>("",    "bd-write-image-command"         );

    WidgetVariable          deviceName                 = new WidgetVariable<String>("",    "device-name"                    );
    WidgetVariable          deviceRequestVolumeCommand = new WidgetVariable<String>("",    "device-request-volume-command"  );
    WidgetVariable          deviceUnloadCommand        = new WidgetVariable<String>("",    "device-unload-volume-command"   );
    WidgetVariable          deviceLoadCommand          = new WidgetVariable<String>("",    "device-load-volume-command"     );
    WidgetVariable          deviceVolumeSize           = new WidgetVariable<String>("",    "device-volume-size"             );
    WidgetVariable          deviceImagePreCommand      = new WidgetVariable<String>("",    "device-image-pre-command"       );
    WidgetVariable          deviceImagePostCommand     = new WidgetVariable<String>("",    "device-image-post-command"      );
    WidgetVariable          deviceImageCommandCommand  = new WidgetVariable<String>("",    "device-image-command"           );
    WidgetVariable          deviceECCPreCommand        = new WidgetVariable<String>("",    "device-ecc-pre-command"         );
    WidgetVariable          deviceECCPostCommand       = new WidgetVariable<String>("",    "device-ecc-post-command"        );
    WidgetVariable          deviceECCCommand           = new WidgetVariable<String>("",    "device-ecc-command"             );
    WidgetVariable          deviceWritePreCommand      = new WidgetVariable<String>("",    "device-write-pre-command"       );
    WidgetVariable          deviceWritePostCommand     = new WidgetVariable<String>("",    "device-write-post-command"      );
    WidgetVariable          deviceWriteCommand         = new WidgetVariable<String>("",    "device-write-command"           );

    WidgetVariable          serverPort                 = new WidgetVariable<Integer>(0,    "server-port"                    );
    WidgetVariable          serverTLSPort              = new WidgetVariable<Integer>(0,    "server-tls-port"                );
    WidgetVariable          serverCAFile               = new WidgetVariable<String>("",    "server-ca-file"                 );
    WidgetVariable          serverCertFile             = new WidgetVariable<String>("",    "server-cert-file"               );
    WidgetVariable          serverKeyFile              = new WidgetVariable<String>("",    "server-key-file"                );
    WidgetVariable          serverPassword             = new WidgetVariable<String>("",    "server-password"                );
    WidgetVariable          servers                    = new WidgetVariable((Object)null);

    WidgetVariable          log                        = new WidgetVariable<String>("",    "log"                            );
    WidgetVariable          logFile                    = new WidgetVariable<String>("",    "log-file"                       );
    WidgetVariable          logFormat                  = new WidgetVariable<String>("",    "log-format"                     );
    WidgetVariable          logPostCommand             = new WidgetVariable<String>("",    "log-post-command"               );

    HashMap<Integer,ServerData> serverDataMap          = new HashMap<Integer,ServerData>();

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Server settings"),700,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    tabFolder = Widgets.newTabFolder(dialog);
    Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);
    final Table  widgetServerTable;
    final Button widgetSave;

    // general
    composite = Widgets.addTab(tabFolder,BARControl.tr("General"));
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,4);
    {
      row = 0;

      label = Widgets.newLabel(composite,BARControl.tr("Temporary directory")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,0.0}));
      Widgets.layout(subComposite,row,1,TableLayoutData.WE);
      {
        subSubComposite = BARWidgets.newDirectory(subComposite,
                                                  BARControl.tr("Path to temporary directory."),
                                                  tmpDirectory
                                                 );
        Widgets.layout(subSubComposite,0,0,TableLayoutData.WE);

        label = Widgets.newLabel(subComposite,BARControl.tr("Max. size")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        combo = BARWidgets.newNumber(subComposite,
                                     BARControl.tr("Size limit for temporary files."),
                                     maxTmpSize,
                                     new String[]{"32M","64M","128M","140M","256M","280M","512M","600M","1G","2G","4G","8G","64G","128G","512G","1T","2T","4T","8T"}
                                    );
        Widgets.layout(combo,0,2,TableLayoutData.WE);
      }
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Nice level")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Process nice level."),
                                     niceLevel,
                                     0,
                                     19
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Max. number of threads")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Max. number of compression and encryption threads."),
                                     maxThreads,
                                     0,
                                     65535
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      combo = BARWidgets.newNumber(composite,
                                   BARControl.tr("Max. band width to use [bits/s]."),
                                   maxBandWidth,
                                   new String[]{"0","64K","128K","256K","512K","1M","2M","4M","8M","16M","32M","64M","128M","256M","512M","1G","10G"}
                                  );
      Widgets.layout(combo,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Min. compress size")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      combo = BARWidgets.newNumber(composite,
                                   BARControl.tr("Min. size of files for compression [bytes]."),
                                   compressMinSize,
                                   new String[]{"0","32","64","128","256","512","1K","4K","8K"}
                                  );
      Widgets.layout(combo,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Jobs directory")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newDirectory(composite,
                                                BARControl.tr("Jobs directory."),
                                                serverJobsDirectory
                                               );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Index database")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("Index database."),
                                           indexDatabase,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      button = BARWidgets.newCheckbox(composite,
                                      BARControl.tr("Auto index udate database."),
                                      indexDatabaseAutoUpdate,
                                      "Auto index update"
                                     );
      Widgets.layout(button,row,1,TableLayoutData.W);
      row++;
    }

    // servers
    composite = Widgets.addTab(tabFolder,BARControl.tr("Servers"));
    composite.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,0.0,0.0,0.0,1.0},new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      row = 0;

      label = Widgets.newLabel(composite,BARControl.tr("Port")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      spinner = BARWidgets.newNumber(composite,
                                     BARControl.tr("Server port number."),
                                     serverPort,
                                     0,
                                     65535
                                    );
      Widgets.layout(spinner,row,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("CA file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("CA file."),
                                           serverCAFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Cert file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("Cert file."),
                                           serverCertFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Key file")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      subSubComposite = BARWidgets.newFile(composite,
                                           BARControl.tr("Key file."),
                                           serverKeyFile,
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*"
                                          );
      Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Password")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      text = BARWidgets.newPassword(composite,
                                    BARControl.tr("CA file."),
                                    serverPassword
                                   );
      Widgets.layout(text,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(composite,BARControl.tr("Servers")+":");
      Widgets.layout(label,row,0,TableLayoutData.W);
      row++;

      widgetServerTable = Widgets.newTable(composite);
      Widgets.layout(widgetServerTable,row,0,TableLayoutData.NSWE,0,2);
      tableColumn = Widgets.addTableColumn(widgetServerTable,0,BARControl.tr("Type"),SWT.LEFT,60);
      tableColumn.setToolTipText(BARControl.tr("Click to sort by name."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      tableColumn = Widgets.addTableColumn(widgetServerTable,1,BARControl.tr("Name"),SWT.LEFT,512,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort by name."));
      tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
      row++;

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(1.0,0.0,2));
      Widgets.layout(subComposite,row,0,TableLayoutData.E,0,2);
      {
        button = Widgets.newButton(subComposite,BARControl.tr("Add"));
        Widgets.layout(button,0,0,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
Dprintf.dprintf("");
          }
        });

        button = Widgets.newButton(subComposite,BARControl.tr("Clone"));
        Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
Dprintf.dprintf("");
          }
        });

        button = Widgets.newButton(subComposite,BARControl.tr("Remove"));
        Widgets.layout(button,0,2,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
Dprintf.dprintf("");
          }
        });
      }
      row++;
    }

    // commands
    composite = Widgets.addTab(tabFolder,BARControl.tr("Commands"));
    composite.setLayout(new TableLayout(1.0,1.0,2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,4);
    {
      subTabFolder = Widgets.newTabFolder(composite);
      Widgets.layout(subTabFolder,0,0,TableLayoutData.NSWE);

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("CD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD device name."),
                                             cdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request volumn command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request volumn command."),
                                             cdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload CD."),
                                             cdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load CD."),
                                             cdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating CD image."),
                                             cdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating CD image."),
                                             cdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             cdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             cdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             cdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             cdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing CD."),
                                             cdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing CD."),
                                             cdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD write command."),
                                             cdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("CD image write command."),
                                             cdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("DVD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;
        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD device name."),
                                             dvdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request volumn command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request volumn command."),
                                             dvdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload DVD."),
                                             dvdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load DVD."),
                                             dvdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating DVD image."),
                                             dvdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating DVD image."),
                                             dvdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             dvdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             dvdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             dvdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             dvdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing DVD."),
                                             dvdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing DVD."),
                                             dvdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD write command."),
                                             dvdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("DVD image write command."),
                                             dvdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("BD"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD device name."),
                                             bdDevice,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,row,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request volumn command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request volumn command."),
                                             bdRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload BD."),
                                             bdUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load BD."),
                                             bdLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating BD image."),
                                             bdImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating BD image."),
                                             bdImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             bdImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             bdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             bdECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             bdECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing BD."),
                                             bdWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing BD."),
                                             bdWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD write command."),
                                             bdWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("BD image write command."),
                                             bdWriteImageCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }

      subComposite = Widgets.addTab(subTabFolder,BARControl.tr("Device"));
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(subComposite,0,0,TableLayoutData.NSWE,0,0,4);
      {
        row = 0;

        label = Widgets.newLabel(subComposite,BARControl.tr("Device")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Device name."),
                                             deviceName,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Request volumn command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Request volumn command."),
                                             deviceRequestVolumeCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Unload command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to unload device."),
                                             deviceUnloadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Load command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to load device."),
                                             deviceLoadCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating device image."),
                                             deviceImagePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating device image."),
                                             deviceImagePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Image command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Image create command."),
                                             deviceImageCommandCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before creating error correction codes."),
                                             deviceECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after creating error correction codes."),
                                             deviceECCPreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("ECC command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to create error correction codes."),
                                             deviceECCCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write pre-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute before writing device."),
                                             deviceWritePreCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write post-command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("Command to execute after writing device."),
                                             deviceWritePostCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(subComposite,BARControl.tr("Write command")+":");
        Widgets.layout(label,row,0,TableLayoutData.W);
        subSubComposite = BARWidgets.newFile(subComposite,
                                             BARControl.tr("device write command."),
                                             deviceWriteCommand,
                                             new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                         },
                                             "*"
                                            );
        Widgets.layout(subSubComposite,row,1,TableLayoutData.WE);
        row++;
      }
    }

    // log
    composite = Widgets.addTab(tabFolder,BARControl.tr("Log"));
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Log")+":");
      Widgets.layout(label,0,0,TableLayoutData.NW);
      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,0.0}));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        row = 0;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log nothing."),
                                        log,
                                        "none",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.isEmpty() || values.contains("none");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            if (checked)
                                            {
                                              widgetVariable.set("none");
                                            }
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log errors."),
                                        log,
                                        "errors",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("errors");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("errors");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("errors");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log warnings."),
                                        log,
                                        "warnings",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("warnings");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("warnings");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("warnings");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log stored/restored files."),
                                        log,
                                        "ok",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("ok");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("ok");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("ok");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log unknown files."),
                                        log,
                                        "unknown files",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("unknown");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("unknown");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("unknown");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log skipped files."),
                                        log,
                                        "skipped files",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("skipped");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("skipped");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("skipped");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log missing files."),
                                        log,
                                        "missing files",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("missing");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("missing");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("missing");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log incomplete files."),
                                        log,
                                        "incomplete files",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("incomplete");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("incomplete");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("incomplete");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log excluded files."),
                                        log,
                                        "excluded files",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("excluded");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("excluded");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("excluded");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log storage operations."),
                                        log,
                                        "storage operations",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("storage");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("storage");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("storage");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log index operations."),
                                        log,
                                        "index operations",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("index");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("index");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("index");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("Log continous operations."),
                                        log,
                                        "continous operations",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("continous");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            if (checked)
                                            {
                                              values.add("continous");
                                              values.remove("none");
                                              values.remove("all");
                                            }
                                            else
                                            {
                                              values.remove("continous");
                                            }

                                            widgetVariable.set(StringUtils.join(values,","));
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;

        button = BARWidgets.newCheckbox(subComposite,
                                        BARControl.tr("all"),
                                        log,
                                        "all",
                                        new BARWidgets.Listener()
                                        {
                                          public boolean getChecked(WidgetVariable widgetVariable)
                                          {
                                            HashSet<String> values = new HashSet<String>(Arrays.asList(StringUtils.split(widgetVariable.getString(),",")));

                                            return values.contains("all");
                                          }
                                          public void setChecked(WidgetVariable widgetVariable, boolean checked)
                                          {
                                            if (checked)
                                            {
                                              widgetVariable.set("all");
                                            }
                                          }
                                        }
                                       );
        Widgets.layout(button,row,0,TableLayoutData.W);
        row++;
      }

      label = Widgets.newLabel(composite,BARControl.tr("Log file")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      subComposite = BARWidgets.newFile(composite,
                                        BARControl.tr("Log file name."),
                                        logFile,
                                        new String[]{BARControl.tr("Log files"),"*.log",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*.log"
                                       );
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Log format")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      text = BARWidgets.newText(composite,
                                BARControl.tr("Log format string."),
                                logFormat
                               );
      Widgets.layout(text,2,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Log post command")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);
      subComposite = BARWidgets.newFile(composite,
                                        BARControl.tr("Log post command."),
                                        logPostCommand,
                                        new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*"
                                       );
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetSave = Widgets.newButton(composite,BARControl.tr("Save"));
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,true);
      }
    });

    BARServer.getServerOption(tmpDirectory               );
    BARServer.getServerOption(maxTmpSize                 );
    BARServer.getServerOption(niceLevel                  );
    BARServer.getServerOption(maxThreads                 );
    BARServer.getServerOption(maxBandWidth               );
    BARServer.getServerOption(compressMinSize            );

    BARServer.getServerOption(indexDatabase              );
    BARServer.getServerOption(indexDatabaseAutoUpdate    );
//    BARServer.getServerOption(indexDatabaseMaxBandWidth  );
    BARServer.getServerOption(indexDatabaseKeepTime      );

    BARServer.getServerOption(cdDevice                   );
    BARServer.getServerOption(cdRequestVolumeCommand     );
    BARServer.getServerOption(cdUnloadCommand            );
    BARServer.getServerOption(cdLoadCommand              );
    BARServer.getServerOption(cdVolumeSize               );
    BARServer.getServerOption(cdImagePreCommand          );
    BARServer.getServerOption(cdImagePostCommand         );
    BARServer.getServerOption(cdImageCommandCommand      );
    BARServer.getServerOption(cdECCPreCommand            );
    BARServer.getServerOption(cdECCPostCommand           );
    BARServer.getServerOption(cdECCCommand               );
    BARServer.getServerOption(cdWritePreCommand          );
    BARServer.getServerOption(cdWritePostCommand         );
    BARServer.getServerOption(cdWriteCommand             );
    BARServer.getServerOption(cdWriteImageCommand        );

    BARServer.getServerOption(dvdDevice                  );
    BARServer.getServerOption(dvdRequestVolumeCommand    );
    BARServer.getServerOption(dvdUnloadCommand           );
    BARServer.getServerOption(dvdLoadCommand             );
    BARServer.getServerOption(dvdVolumeSize              );
    BARServer.getServerOption(dvdImagePreCommand         );
    BARServer.getServerOption(dvdImagePostCommand        );
    BARServer.getServerOption(dvdImageCommandCommand     );
    BARServer.getServerOption(dvdECCPreCommand           );
    BARServer.getServerOption(dvdECCPostCommand          );
    BARServer.getServerOption(dvdECCCommand              );
    BARServer.getServerOption(dvdWritePreCommand         );
    BARServer.getServerOption(dvdWritePostCommand        );
    BARServer.getServerOption(dvdWriteCommand            );
    BARServer.getServerOption(dvdWriteImageCommand       );

    BARServer.getServerOption(bdDevice                   );
    BARServer.getServerOption(bdRequestVolumeCommand     );
    BARServer.getServerOption(bdUnloadCommand            );
    BARServer.getServerOption(bdLoadCommand              );
    BARServer.getServerOption(bdVolumeSize               );
    BARServer.getServerOption(bdImagePreCommand          );
    BARServer.getServerOption(bdImagePostCommand         );
    BARServer.getServerOption(bdImageCommandCommand      );
    BARServer.getServerOption(bdECCPreCommand            );
    BARServer.getServerOption(bdECCPostCommand           );
    BARServer.getServerOption(bdECCCommand               );
    BARServer.getServerOption(bdWritePreCommand          );
    BARServer.getServerOption(bdWritePostCommand         );
    BARServer.getServerOption(bdWriteCommand             );
    BARServer.getServerOption(bdWriteImageCommand        );

    BARServer.getServerOption(deviceName                 );
    BARServer.getServerOption(deviceRequestVolumeCommand );
    BARServer.getServerOption(deviceUnloadCommand        );
    BARServer.getServerOption(deviceLoadCommand          );
    BARServer.getServerOption(deviceVolumeSize           );
    BARServer.getServerOption(deviceImagePreCommand      );
    BARServer.getServerOption(deviceImagePostCommand     );
    BARServer.getServerOption(deviceImageCommandCommand  );
    BARServer.getServerOption(deviceECCPreCommand        );
    BARServer.getServerOption(deviceECCPostCommand       );
    BARServer.getServerOption(deviceECCCommand           );
    BARServer.getServerOption(deviceWritePreCommand      );
    BARServer.getServerOption(deviceWritePostCommand     );
    BARServer.getServerOption(deviceWriteCommand         );

    BARServer.getServerOption(serverPort                 );
    BARServer.getServerOption(serverTLSPort              );
    BARServer.getServerOption(serverCAFile               );
    BARServer.getServerOption(serverCertFile             );
    BARServer.getServerOption(serverKeyFile              );
    BARServer.getServerOption(serverPassword             );
    BARServer.getServerOption(serverJobsDirectory        );

    BARServer.getServerOption(log                        );
    BARServer.getServerOption(logFile                    );
    BARServer.getServerOption(logFormat                  );
    BARServer.getServerOption(logPostCommand             );

    ServerDataComparator serverDataComparator = new ServerDataComparator(widgetServerTable);
    ServerData           serverData;

    Widgets.removeAllTableItems(widgetServerTable);

    serverData = new ServerData(0,"default",ServerTypes.FTP);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            (Object)serverData,
                            ServerTypes.FTP.toString(),
                            "default"
                           );
    serverDataMap.put(0,serverData);
    serverData = new ServerData(0,"default",ServerTypes.SSH);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            (Object)serverData,
                            ServerTypes.SSH.toString(),
                            "default"
                           );
    serverDataMap.put(0,serverData);
    serverData = new ServerData(0,"default",ServerTypes.WEBDAV);
    Widgets.insertTableItem(widgetServerTable,
                            serverDataComparator,
                            (Object)serverData,
                            ServerTypes.WEBDAV.toString(),
                            "default"
                           );
    serverDataMap.put(0,serverData);

    String[]             resultErrorMessage   = new String[1];
    ArrayList<ValueMap>  resultMapList        = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("SERVER_LIST"),
                                         0,
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }
    for (ValueMap resultMap : resultMapList)
    {
      // get data
      int          id                 = resultMap.getInt   ("id"                          );
      String       name               = resultMap.getString("name"                        );
      ServerTypes  serverType         = resultMap.getEnum  ("serverType",ServerTypes.class);
      String       loginName          = resultMap.getString("loginName"                   );
      int          maxConnectionCount = resultMap.getInt   ("maxConnectionCount"          );
      long         maxStorageSize     = resultMap.getLong  ("maxStorageSize"              );

      // create server data
      serverData = new ServerData(id,name,serverType);
      serverDataMap.put(id,serverData);

      // add table entry
      Widgets.insertTableItem(widgetServerTable,
                              serverDataComparator,
                              (Object)serverData,
                              serverType.toString(),
                              name
                             );
    }

    if ((Boolean)Dialogs.run(dialog,false))
    {
      final String[] errorMessage = new String[1];

      BARServer.setServerOption(tmpDirectory,errorMessage);
      BARServer.setServerOption(maxTmpSize,errorMessage);
      BARServer.setServerOption(niceLevel,errorMessage);
      BARServer.setServerOption(maxThreads,errorMessage);
      BARServer.setServerOption(maxBandWidth,errorMessage);
      BARServer.setServerOption(compressMinSize,errorMessage);

      BARServer.setServerOption(indexDatabase,errorMessage);
      BARServer.setServerOption(indexDatabaseAutoUpdate,errorMessage);
//      BARServer.setServerOption(indexDatabaseMaxBandWidth,errorMessage);
      BARServer.setServerOption(indexDatabaseKeepTime,errorMessage);

      BARServer.setServerOption(cdDevice                   );
      BARServer.setServerOption(cdRequestVolumeCommand     );
      BARServer.setServerOption(cdUnloadCommand            );
      BARServer.setServerOption(cdLoadCommand              );
      BARServer.setServerOption(cdVolumeSize               );
      BARServer.setServerOption(cdImagePreCommand          );
      BARServer.setServerOption(cdImagePostCommand         );
      BARServer.setServerOption(cdImageCommandCommand      );
      BARServer.setServerOption(cdECCPreCommand            );
      BARServer.setServerOption(cdECCPostCommand           );
      BARServer.setServerOption(cdECCCommand               );
      BARServer.setServerOption(cdWritePreCommand          );
      BARServer.setServerOption(cdWritePostCommand         );
      BARServer.setServerOption(cdWriteCommand             );
      BARServer.setServerOption(cdWriteImageCommand        );

      BARServer.setServerOption(dvdDevice                  );
      BARServer.setServerOption(dvdRequestVolumeCommand    );
      BARServer.setServerOption(dvdUnloadCommand           );
      BARServer.setServerOption(dvdLoadCommand             );
      BARServer.setServerOption(dvdVolumeSize              );
      BARServer.setServerOption(dvdImagePreCommand         );
      BARServer.setServerOption(dvdImagePostCommand        );
      BARServer.setServerOption(dvdImageCommandCommand     );
      BARServer.setServerOption(dvdECCPreCommand           );
      BARServer.setServerOption(dvdECCPostCommand          );
      BARServer.setServerOption(dvdECCCommand              );
      BARServer.setServerOption(dvdWritePreCommand         );
      BARServer.setServerOption(dvdWritePostCommand        );
      BARServer.setServerOption(dvdWriteCommand            );
      BARServer.setServerOption(dvdWriteImageCommand       );

      BARServer.setServerOption(bdDevice                   );
      BARServer.setServerOption(bdRequestVolumeCommand     );
      BARServer.setServerOption(bdUnloadCommand            );
      BARServer.setServerOption(bdLoadCommand              );
      BARServer.setServerOption(bdVolumeSize               );
      BARServer.setServerOption(bdImagePreCommand          );
      BARServer.setServerOption(bdImagePostCommand         );
      BARServer.setServerOption(bdImageCommandCommand      );
      BARServer.setServerOption(bdECCPreCommand            );
      BARServer.setServerOption(bdECCPostCommand           );
      BARServer.setServerOption(bdECCCommand               );
      BARServer.setServerOption(bdWritePreCommand          );
      BARServer.setServerOption(bdWritePostCommand         );
      BARServer.setServerOption(bdWriteCommand             );
      BARServer.setServerOption(bdWriteImageCommand        );

      BARServer.setServerOption(deviceName                 );
      BARServer.setServerOption(deviceRequestVolumeCommand );
      BARServer.setServerOption(deviceUnloadCommand        );
      BARServer.setServerOption(deviceLoadCommand          );
      BARServer.setServerOption(deviceVolumeSize           );
      BARServer.setServerOption(deviceImagePreCommand      );
      BARServer.setServerOption(deviceImagePostCommand     );
      BARServer.setServerOption(deviceImageCommandCommand  );
      BARServer.setServerOption(deviceECCPreCommand        );
      BARServer.setServerOption(deviceECCPostCommand       );
      BARServer.setServerOption(deviceECCCommand           );
      BARServer.setServerOption(deviceWritePreCommand      );
      BARServer.setServerOption(deviceWritePostCommand     );
      BARServer.setServerOption(deviceWriteCommand         );

      BARServer.setServerOption(serverPort                 );
      BARServer.setServerOption(serverTLSPort              );
      BARServer.setServerOption(serverCAFile               );
      BARServer.setServerOption(serverCertFile             );
      BARServer.setServerOption(serverKeyFile              );
      BARServer.setServerOption(serverPassword             );
      BARServer.setServerOption(serverJobsDirectory        );

      BARServer.setServerOption(log,errorMessage           );
      BARServer.setServerOption(logFile,errorMessage       );
      BARServer.setServerOption(logFormat,errorMessage     );
      BARServer.setServerOption(logPostCommand,errorMessage);

      if (BARServer.flushServerOption(errorMessage) != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Flush server options fail (error: {0})",errorMessage[0]));
      }

    }
  }
}

/* end of file */
