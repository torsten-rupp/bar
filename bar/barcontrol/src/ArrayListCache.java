/***********************************************************************\
*
* $Revision: 1920 $
* $Date: 2017-03-17 13:12:24 +0100 (Fri, 17 Mar 2017) $
* $Author: torsten $
* Contents: cache info
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.ArrayList;
import java.util.HashMap;

/****************************** Classes ********************************/

/** array list cache
 */
public class ArrayListCache<T>
{
  private ArrayList<T> data;
  private long         lastUpdate;

  ArrayListCache()
  {
    data       = new ArrayList<T>();
    lastUpdate = 0;
  }

  public ArrayList<T> getData()
  {
    return data;
  }

  public boolean isExpired(long time)
  {
    return System.currentTimeMillis() > lastUpdate+time;
  }

  public void updated()
  {
    lastUpdate = System.currentTimeMillis();
  }
}

/** array list cache map
 */
class ArrayListCacheMap<T> extends HashMap<String,ArrayListCache<T>>
{
  public ArrayListCache<T> get(String key)
  {
    ArrayListCache<T> arrayListCache = super.get(key);
    if (arrayListCache == null)
    {
      arrayListCache = new ArrayListCache();
      put(key,arrayListCache);
    }

    return arrayListCache;
  }
}

/* end of file */
