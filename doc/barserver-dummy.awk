function send(id,done,error, data)
{
  print id,done,error,data;
  if (debugFlag)
  {
    print "Sent:",id,done,error,data > "/dev/stderr";
  }
}

BEGIN {
  debugFlag=1
  restoreSelectFlag=0
  print "SESSION id=22c8d237c43fff83854286ec7473207a5b6fea7ab17c511c4e3f0593c4f36bd464f923fcef2ff8f95c481c73a1c2330d9488e3d7f0ca412bd1c841105882c199 encryptTypes=RSA,NONE n=00BA6067D81E677FCB74A8C08FFF2184174E7FFCDCAA0D941F20F4FAC85FA3E8D0E45E99CDDDA1F616C396B6E31923CF962F6E6ADF1F8BF42047738143ADB8BBA1685CEB4D1E977A7B42CF68B5D1E9F99CF8C4A3F5AF351937397DE3FF7A63F43D9580AC7CB2357578D3F37291FC27842146E9AD7A2BF6A3AB2E0F7D161B05966D e=010001";
}

/.*/ {
  print "Received:",$0 > "/dev/stderr";
}

/^[0-9]+ START_TLS/ {
  print $1,1,1;
  next;
}
/^[0-9]+ AUTHORIZE/ {
  send($1,1,0);
  next;
}
/^[0-9]+ VERSION/ {
  send($1,1,0,"major=6 minor=0 mode=MASTER");
  next;
}
/^[0-9]+ GET name=PATH_SEPARATOR/ {
  send($1,1,0,"value='/'");
  next;
}
/^[0-9]+ ROOT_LIST/ {
  send($1,1,0,"name='/' size=0");
  next;
}
/^[0-9]+ DEVICE_LIST/ {
  send($1,0,0,"name='/dev/sda' size=0 mounted=no");
  send($1,0,0,"name='/dev/sda1' size=0 mounted=no");
  send($1,0,0,"name='/dev/sda2' size=0 mounted=no");
  send($1,0,0,"name='/dev/sda3' size=0 mounted=no");
  send($1,0,0,"name='/dev/sda4' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdb' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdb1' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdb2' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdb3' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdb4' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdc' size=0 mounted=no");
  send($1,0,0,"name='/dev/sdc1' size=0 mounted=no");
  send($1,1,0,"name='/dev/sdc2' size=0 mounted=no");
  next;
}
/^[0-9]+ JOB_LIST/ {
  send($1,0,0,"jobUUID=1 \
               master='' \
               name='home' \
               state=DONE \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,0,0,"jobUUID=2 \
               master='' \
               name='home-webdav' \
               state=NONE \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,0,0,"jobUUID=3 \
               master='' \
               name='system' \
               state=RUNNING \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,0,0,"jobUUID=4 \
               master='' \
               name='system-dvd' \
               state=NONE \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,0,0,"jobUUID=7 \
               master='' \
               name='projects' \
               state=NONE \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,0,0,"jobUUID=8 \
               master='' \
               name='projects-ftp' \
               state=WAITING \
               slaveHostName='' \
               slaveHostPort=0 \
               slaveHostForceSSL=no \
               slaveState=OFFLINE \
               archiveType=FULL \
               archivePartSize=1000000 \
               deltaCompressAlgorithm=none \
               byteCompressAlgorithm=none \
               cryptAlgorithm=none \
               cryptType=symmetic \
               cryptPasswordMode=ask \
               lastExecutedDateTime=0 \
               lastErrorMessage='' estimatedRestTime=0 \
              ");
  send($1,1,0);
  next;
}
/^[0-9]+ JOB_FLUSH/ {
  next;
}
/^[0-9]+ STATUS/ {
  send($1,1,0,"state=RUNNING time=0");
  next;
}
/^[0-9]+ MASTER_GET/ {
  send($1,1,0,"name=''");
  next;
}
/^[0-9]+ SCHEDULE_OPTION_GET/ {
  send($1,1,0,"value=''");
  next;
}
/^[0-9]+ SCHEDULE_OPTION_SET/ {
  send($1,1,0);
  next;
}

/^[0-9]+ FILE_LIST/ {
  send($1,0,0,"fileType=DIRECTORY name='/lib32' dateTime=1545743118 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/users' dateTime=1410217844 noBackup=no noDump=no");
  send($1,0,0,"fileType=LINK destinationFileType=FILE name='/vmlinuz' dateTime=1581837535 noDump=no");
  send($1,0,0,"fileType=LINK destinationFileType=FILE name='/libnss3.so' dateTime=1456511116 noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/sys' dateTime=1582376694 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/home' dateTime=1558783718 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/lib64' dateTime=1545745666 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/run' dateTime=1582186901 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/root' dateTime=1579364459 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/libx32' dateTime=1545738361 noBackup=no noDump=no");
  send($1,0,0,"fileType=LINK destinationFileType=FILE name='/initrd.img' dateTime=1581837535 noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/bin' dateTime=1575219931 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/opt' dateTime=1576762761 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/usr' dateTime=1572339470 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/proc' dateTime=1581800590 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/media' dateTime=1580594358 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/sbin' dateTime=1579334389 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/mnt' dateTime=1545730568 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/boot' dateTime=1581837578 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/man' dateTime=1307904011 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/etc' dateTime=1580572811 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/dev' dateTime=1581866153 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/lib' dateTime=1579334389 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/lost+found' dateTime=1307029799 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/tmp' dateTime=1582378742 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/var' dateTime=1545769746 noBackup=no noDump=no");
  send($1,0,0,"fileType=DIRECTORY name='/srv' dateTime=1356987138 noBackup=no no");
  send($1,1,0);
  next;
}
/^[0-9]+ INCLUDE_LIST/ {
  send($1,0,0,"id=51 entryType=file pattern='/etc' patternType=glob");
  send($1,0,0,"id=52 entryType=file pattern='/selinux' patternType=glob");
  send($1,0,0,"id=53 entryType=file pattern='/dev' patternType=glob");
  send($1,0,0,"id=54 entryType=file pattern='/man' patternType=glob");
  send($1,0,0,"id=55 entryType=file pattern='/vmlinuz' patternType=glob");
  send($1,0,0,"id=56 entryType=file pattern='/root' patternType=glob");
  send($1,0,0,"id=57 entryType=file pattern='/lib' patternType=glob");
  send($1,0,0,"id=58 entryType=file pattern='/.config' patternType=glob");
  send($1,0,0,"id=59 entryType=file pattern='/srv' patternType=glob");
  send($1,0,0,"id=60 entryType=file pattern='/initrd.img' patternType=glob");
  send($1,0,0,"id=61 entryType=file pattern='/lib64' patternType=glob");
  send($1,0,0,"id=62 entryType=file pattern='/lib32' patternType=glob");
  send($1,0,0,"id=63 entryType=file pattern='/bin' patternType=glob");
  send($1,0,0,"id=64 entryType=file pattern='/usr' patternType=glob");
  send($1,0,0,"id=65 entryType=file pattern='/boot' patternType=glob");
  send($1,0,0,"id=66 entryType=file pattern='/sbin' patternType=glob");
  send($1,0,0,"id=67 entryType=file pattern='/opt' patternType=glob");
  send($1,0,0,"id=68 entryType=file pattern='/var' patternType=glob");
  send($1,1,0);
  next;
}
/^[0-9]+ EXCLUDE_LIST/ {
  send($1,0,0,"id=461 pattern='/var/lib/bar/index.db*' patternType=glob");
  send($1,0,0,"id=461 pattern='/dev' patternType=glob");
  send($1,1,0);
  next;
}
/^[0-9]+ EXCLUDE_COMPRESS_LIST/ {
  send($1,0,0,"pattern='*.gz' patternType=glob");
  send($1,0,0,"pattern='*.tgz' patternType=glob");
  send($1,0,0,"pattern='*.bz' patternType=glob");
  send($1,0,0,"pattern='*.bz2' patternType=glob");
  send($1,0,0,"pattern='*.gzip' patternType=glob");
  send($1,0,0,"pattern='*.lzma' patternType=glob");
  send($1,0,0,"pattern='*.zip' patternType=glob");
  send($1,0,0,"pattern='*.rar' patternType=glob");
  send($1,0,0,"pattern='*.7z' patternType=glob");
  send($1,1,0);
  next;
}
/^[0-9]+ MOUNT_LIST/ {
  send($1,0,0,"id=110 name='/media/backup' device=''");
  send($1,1,0);
  next;
}
/^[0-9]+ SOURCE_LIST/ {
  send($1,1,0);
  next;
}
/^[0-9]+ SCHEDULE_LIST/ {
  send($1,0,0,"scheduleUUID=5e1cbe0b-f54b-45bc-89c1-469111e81ee3 archiveType=full date=*-*-01 weekDays=* time=23:00 interval=0 beginTime=*:* endTime=*:* customText='' testCreatedArchives=no noStorage=no enabled=yes lastExecutedDateTime=0 totalEntities=0 totalStorageCount=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 archiveType=incremental date=*-*-* weekDays=* time=23:00 interval=0 beginTime=*:* endTime=*:* customText='' testCreatedArchives=no noStorage=no enabled=yes lastExecutedDateTime=1582346747 totalEntities=12 totalStorageCount=7 totalEntryCount=0 totalEntrySize=0");
  send($1,1,0);
  next;
}
/^[0-9]+ PERSISTENCE_LIST/ {
  send($1,0,0,"persistenceId=111 archiveType=incremental minKeep=1 maxKeep=4 maxAge=28 size=0");
  send($1,0,0,"persistenceId=111 entityId=3330 createdDateTime=1582342898 size=181545558 totalEntryCount=5493 totalEntrySize=1659806641 inTransit=no");
  send($1,0,0,"persistenceId=111 entityId=3138 createdDateTime=1582236736 size=0 totalEntryCount=0 totalEntrySize=0 inTransit=no");
  send($1,0,0,"persistenceId=111 entityId=2994 createdDateTime=1582154300 size=0 totalEntryCount=0 totalEntrySize=0 inTransit=no");
  send($1,0,0,"persistenceId=112 archiveType=full minKeep=1 maxKeep=2 maxAge=90 size=0");
  send($1,0,0,"persistenceId=113 archiveType=full minKeep=1 maxKeep=2 maxAge=180 size=0");
  send($1,0,0,"persistenceId=114 archiveType=full minKeep=1 maxKeep=1 maxAge=365 size=0");
  send($1,1,0);
  next;
}
/^[0-9]+ JOB_STATUS/ {
  send($1,1,0,"state=RUNNING \
               errorCode=0 \
               errorNumber=0 \
               errorData='' \
               doneCount=34995 \
               doneSize=8518263208 \
               totalEntryCount=100539 \
               totalEntrySize=39503921586 \
               collectTotalSumDone=yes \
               skippedEntryCount=6 \
               skippedEntrySize=745 \
               errorEntryCount=2 \
               errorEntrySize=4990 \
               archiveSize=100000 \
               compressionRatio=82 \
               entryName='/usr/bin/bar' \
               entryDoneSize=10944512 \
               entryTotalSize=62174352 \
               storageName='/backup/data/system-003.bar' \
               storageDoneSize=1700460006 \
               storageTotalSize=2058977288 \
               volumeNumber=0 \
               volumeProgress=0 \
               requestedVolumeNumber=0 \
               entriesPerSecond=37 \
               bytesPerSecond=7820540 \
               storageBytesPerSecond=28602424 \
               estimatedRestTime=8343 \
               message='' \
              ");
  next;
}
/^[0-9]+ JOB_OPTION_GET jobUUID=.+ name="archive-part-size"/ {
  send($1,1,0,"value=1073741824");
  next;
}
/^[0-9]+ JOB_OPTION_GET jobUUID=.+ name="archive-name"/ {
  send($1,1,0,"value='/media/backup/system-%T-###.bar'");
  next;
}
/^[0-9]+ JOB_OPTION_GET jobUUID=.+ name="compress-algorithm"/ {
  send($1,1,0,"value=none+lz4-16");
  next;
}
/^[0-9]+ JOB_OPTION_GET jobUUID=.+/ {
  send($1,1,0,"value=''");
  next;
}
/^[0-9]+ SCHEDULE_LIST/ {
  send($1,1,0);
  next;
}

/^[0-9]+ INDEX_LIST_INFO/ {
  if (restoreSelectFlag==0)
  {
    send($1,1,0,"totalStorageCount=4303 totalStorageSize=93534903298 totalEntryCount=1069214 totalEntrySize=363682158906 totalEntryContentSize=0");
  }
  else
  {
    send($1,1,0,"totalStorageCount=4 totalStorageSize=9393298 totalEntryCount=1014 totalEntrySize=36358906 totalEntryContentSize=0");
  }
  next;
}
/^[0-9]+ INDEX_LIST_CLEAR/ {
  restoreSelectFlag=0
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_LIST_ADD/ {
  restoreSelectFlag=1
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_ENTRY_LIST_CLEAR/ {
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_UUID_LIST/ {
  send($1,0,0,"uuidId=49 jobUUID=c3254f37-77a9-49e4-a89a-a42b9bfe999d name='home' lastExecutedDateTime=1582360203 lastErrorMessage='' totalSize=16300722702 totalEntryCount=129056 totalEntrySize=56126801090");
  send($1,0,0,"uuidId=49 jobUUID=c3254f37-77a9-49e4-a89a-a42b9bfe999d name='projects-hd' lastExecutedDateTime=1582370203 lastErrorMessage='' totalSize=16300722702 totalEntryCount=129056 totalEntrySize=56126801090");
  send($1,0,0,"uuidId=81 jobUUID=a4cf9808-6a0c-482a-a9c1-4afb1deb0113 name='projects-webdav' lastExecutedDateTime=1582371051 lastErrorMessage='' totalSize=9287385764 totalEntryCount=100554 totalEntrySize=39621108735");
  send($1,0,0,"uuidId=113 jobUUID=405a50b0-e9cf-41ab-833e-91024c2237cb name='mail' lastExecutedDateTime=1582342028 lastErrorMessage='' totalSize=2669529474 totalEntryCount=55571 totalEntrySize=3729023628");
  send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 name='system-hd' lastExecutedDateTime=1582342898 lastErrorMessage='' totalSize=879566294 totalEntryCount=34860 totalEntrySize=6319439693");
  send($1,0,0,"uuidId=161 jobUUID=46105e1f-0d6d-48de-ba91-03d7d6e6d60e name='pictures' lastExecutedDateTime=1582355795 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=d65bf4f4-c284-425c-8eb9-84b169411e2e name='projects-deltanetworks' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=c86619cf-0f28-42f3-8426-e82b24d6463e name='projects-webdav-gmx' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=315c21f9-7659-4f97-a672-6eaef4d9d8d2 name='projects-continuous' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=4d55e653-a54c-4b1d-810b-7326bac39df4 name='system-dvd' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=338d03b0-7b0c-4a55-a927-c62a69eced1e name='virtualbox' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=3d048506-31b8-4e57-bc40-8c0486ab48d5 name='4share' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,0,0,"uuidId=0 jobUUID=313de035-a8a1-4a93-b56a-46936cfd9982 name='home-music' lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0");
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_ENTITY_LIST/ {
  send($1,0,0,"jobUUID=1 scheduleUUID=1 entityId=2786 archiveType='full' createdDateTime=1582011128 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0 expireDateTime=0");
  send($1,0,0,"jobUUID=1 scheduleUUID=2 entityId=2802 archiveType='full' createdDateTime=1582021128 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0 expireDateTime=0");
  send($1,0,0,"jobUUID=1 scheduleUUID=3 entityId=2818 archiveType='incremental' createdDateTime=1582031128 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0 expireDateTime=0");
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_ENTRY_LIST_INFO/ {
  if (restoreSelectFlag==0)
  {
    send($1,1,0,"totalEntryCount=2147295 totalEntrySize=583330009011 totalEntryContentSize=583330009011");
  }
  else
  {
    send($1,1,0,"totalEntryCount=4 totalEntrySize=54230911 totalEntryContentSize=3309011");
  }
  next;
}
/^[0-9]+ INDEX_ENTRY_LIST/ {
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/grub-editenv' size=245688 dateTime=1581800646 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/pphs' size=404 dateTime=1575219888 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/Xwayland' size=2299608 dateTime=1563291314 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/amd64-mingw32msvc-windmc' size=690680 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/x-window-manager' size=34 dateTime=1581651288 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/shuf' size=55480 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/pslatex' size=54 dateTime=1581651288 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/ssh-add' size=346248 dateTime=1581869139 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/ppmntsc' size=14208 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/ppatcher' size=1989 dateTime=1555224826 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/messages.mailutils' size=10608 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/gsdj' size=352 dateTime=1575219888 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/7z' size=39 dateTime=1561906457 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/lastlog' size=18504 dateTime=1558881633 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/grep-excuses' size=10242 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/make-first-existing-target' size=4905 dateTime=1555224826 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/pldd' size=18656 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/compare-im6.q16' size=10232 dateTime=1575219888 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/perf' size=1622 dateTime=1578513015 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/partitionmanager' size=710800 dateTime=1557950884 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,0,0,"jobName='system' archiveType=full hostName='tooku' entryId=21  entryType=FILE name='/usr/bin/ppmtolj' size=10104 dateTime=1555224827 userId=1001 groupId=1001 permission=33152 fragmentCount=0");
  send($1,1,0);
  next;
}
/^[0-9]+ INDEX_STORAGE_LIST_INFO/ {
  if (restoreSelectFlag==0)
  {
    send($1,1,0,"totalStorageCount=5 totalStorageSize=467330345410 totalEntryCount=34860 totalEntrySize=6319474553 totalEntryContentSize=6319474553");
  }
  else
  {
    send($1,1,0,"totalStorageCount=4 totalStorageSize=3035505 totalEntryCount=4 totalEntrySize=54230911 totalEntryContentSize=3309011");
  }
  next;
}
/^[0-9]+ INDEX_STORAGE_LIST/ {
  if (restoreSelectFlag==0)
  {
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=66 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=67 name='/media/backup/2020-02-15/system-000.bar' dateTime=1581807232 size=171982692 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1581811813 errorMessage='' totalEntryCount=6386 totalEntrySize=1485024576");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=1154 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=14643 name='/media/backup/2020-02-16/system-000.bar' dateTime=1581891395 size=269795718 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1581896270 errorMessage='' totalEntryCount=12010 totalEntrySize=1308150220");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=2562 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=23043 name='/media/backup/2020-02-17/system-000.bar' dateTime=1581977795 size=109502524 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1581982741 errorMessage='' totalEntryCount=5471 totalEntrySize=785522513");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=2866 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=27555 name='/media/backup/2020-02-18/system-000.bar' dateTime=1582093564 size=146739802 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1582097475 errorMessage='' totalEntryCount=5500 totalEntrySize=1080935743");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=3330 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=39635 name='/media/backup/2020-02-21/system-000.bar' dateTime=1582342906 size=181545558 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1582346743 errorMessage='' totalEntryCount=5493 totalEntrySize=1659806641");
  }
  else
  {
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=1154 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=14643 name='/media/backup/2020-02-16/system-000.bar' dateTime=1581891395 size=269795718 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1581896270 errorMessage='' totalEntryCount=12010 totalEntrySize=1308150220");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=2562 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=23043 name='/media/backup/2020-02-17/system-000.bar' dateTime=1581977795 size=109502524 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1581982741 errorMessage='' totalEntryCount=5471 totalEntrySize=785522513");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=2866 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=27555 name='/media/backup/2020-02-18/system-000.bar' dateTime=1582093564 size=146739802 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1582097475 errorMessage='' totalEntryCount=5500 totalEntrySize=1080935743");
    send($1,0,0,"uuidId=129 jobUUID=c39b72b9-c125-411a-a396-f7da6a832ba3 jobName='system-hd' entityId=3330 scheduleUUID=1b6a2f4c-1df1-44e8-b591-5a9951f1c8a9 hostName='tooku' archiveType='incremental' storageId=39635 name='/media/backup/2020-02-21/system-000.bar' dateTime=1582342906 size=181545558 indexState='OK' indexMode='AUTO' lastCheckedDateTime=1582346743 errorMessage='' totalEntryCount=5493 totalEntrySize=1659806641");
  }
  send($1,1,0);
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="tmp-directory"/ {
  send($1,1,0,"value=/tmp");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="max-tmp-size"/ {
  send($1,1,0,"value=50G");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="nice-level"/ {
  send($1,1,0,"value=19");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="max-threads"/ {
  send($1,1,0,"value=32");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="compress-min-size"/ {
  send($1,1,0,"value=64");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="continuous-max-size"/ {
  send($1,1,0,"value=128M");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="index-database"/ {
  send($1,1,0,"value=sqlite3:/var/lib/bar/index.db");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="index-database-update"/ {
  send($1,1,0,"value=yes");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="index-database-auto-update"/ {
  send($1,1,0,"value=yes");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="index-database-keep-time"/ {
  send($1,1,0,"value=7days");
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="cd-device"/ {
  send($1,1,0,"value=/dev/cdrw");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-request-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-unload-volume-command"/ {
  send($1,1,0,"value='eject %device'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-load-volume-command"/ {
  send($1,1,0,"value='eject -t %device'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-volume-size"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-image-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-image-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-image-command"/ {
  send($1,1,0,"value='nice mkisofs -V Backup -volset %number -J -r -o %image %directory'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-ecc-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-ecc-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-ecc-command/ {
  send($1,1,0,"value='nice dvdisaster -mRS03 -x %j1 -c -i %image -v'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-blank-command"/ {
  send($1,1,0,"value='nice dvd+rw-format -force %device'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-write-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-write-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-write-command"/ {
  send($1,1,0,"value='nice sh -c \\'mkisofs -V Backup -volset %number -J -r -o %image %directory && cdrecord dev=%device %image\\''");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="cd-write-image-command"/ {
  send($1,1,0,"value='nice cdrecord dev=%device %image'");
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="dvd-device"/ {
  send($1,1,0,"value=/dev/dvd");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-request-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-unload-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-load-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-volume-size"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-image-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-image-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-image-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-ecc-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-ecc-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-ecc-command/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-blank-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-write-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-write-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-write-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="dvd-write-image-command"/ {
  send($1,1,0,"value=");
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="bd-device"/ {
  send($1,1,0,"value=/dev/bd");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-request-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-unload-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-load-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-volume-size"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-image-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-image-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-image-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-ecc-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-ecc-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-ecc-command/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-blank-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-write-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-write-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-write-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="bd-write-image-command"/ {
  send($1,1,0,"value=");
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="device"/ {
  send($1,1,0,"value=/dev/bd");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-request-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-unload-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-load-volume-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-volume-size"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-image-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-image-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-image-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-ecc-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-ecc-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-ecc-command/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-blank-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-write-pre-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-write-post-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-write-command"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="device-write-image-command"/ {
  send($1,1,0,"value=");
  next;
}

/^[0-9]+ SERVER_OPTION_GET name="server-port"/ {
  send($1,1,0,"value=10000");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="server-tls-port"/ {
  send($1,1,0,"value=10000");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="server-ca-file"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="server-cert-file"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="server-key-file"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="server-password"/ {
  send($1,1,0,"value=");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="jobs-directory"/ {
  send($1,1,0,"value=/etc/bar/jobs");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="verbose"/ {
  send($1,1,0,"value=0");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="log"/ {
  send($1,1,0,"value=errors,warnings,skipped,storage,index");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="log-file/ {
  send($1,1,0,"value='/var/log/bar.log'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="log-format"/ {
  send($1,1,0,"value='%Y-%m-%d %H:%M:%S'");
  next;
}
/^[0-9]+ SERVER_OPTION_GET name="log-post-command"/ {
  send($1,1,0,"value='sh -c \\'cat %file\\'|mail -s \\'Backup log\\' torsten'");
  next;
}
/^[0-9]+ MAINTENANCE_LIST/ {
  send($1,0,0,"id=1 date=*-*-* weekDays=Mo,Tu,We,Th,Fr beginTime=23:00 endTime=05:00");
  send($1,0,0,"id=2 date=*-*-* weekDays=Su beginTime=*:* endTime=*:*");
  send($1,1,0);
  next;
}
/^[0-9]+ SERVER_LIST/ {
  send($1,0,0,"id=1 name='archive' serverType=ftp path='archive.com/backup' loginName=bar port=0 maxConnectionCount=0 maxStorageSize=0");
  send($1,0,0,"id=2 name='cloud-storage' serverType=webdav path='archive.com/backup' loginName=bar port=0 maxConnectionCount=0 maxStorageSize=0");
  send($1,1,0);
  next;
}

/^[0-9]+ ABORT/ {
  send($1,1,0);
  next;
}

/.*/ {
  print "ERROR: unknown command",$0 > "/dev/stderr";
  exit(1);
}

END {
  print "Close" > /dev/stderr;
}
