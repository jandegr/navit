/*
  Navit, a modular navigation system. Copyright (C) 2005-2008 Navit Team

  This program is free software; you can redistribute it and/or modify it under the terms of the
  GNU General Public License version 2 as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
  even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License along with this program; if
  not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
 */

package org.navitproject.navit;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;

import java.lang.reflect.Field;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Locale;


public class NavitAddressSearchActivity extends Activity {

    private static final String TAG = "NavitAddress";
    private static String sAddressString = "";
    private ProgressBar mZoekBar;
    private final int mZoekTypeTown = 2; // in enum steken ?
    private int mZoektype = mZoekTypeTown; // town
    private int mOngoingSearches = 0;
    private ArrayAdapter<NavitSearchAddress> mAddressAdapter;
    private final NavitAddressList mAddressesFound = new NavitAddressList();
    private NavitSearchAddress mSelectedTown;
    private NavitSearchAddress mSelectedStreet;
    private String mCountry;
    private ImageButton mCountryButton;
    private Button mResultActionButton;
    private long mSearchHandle = 0;

    private int getDrawableID(String resourceName) {
        int drawableId = 0;
        try {
            Class<?> res = R.drawable.class;
            Field field = res.getField(resourceName + "_64_64");
            drawableId = field.getInt(null);
        } catch (Exception e) {
            Log.e(TAG, "Failure to get drawable id.", e);
        }
        return drawableId;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // mPartialSearch = last_address_partial_match;
        sAddressString = "";

        getWindow().setFlags(WindowManager.LayoutParams.FLAG_BLUR_BEHIND,
                WindowManager.LayoutParams.FLAG_BLUR_BEHIND);
        LinearLayout panel = new LinearLayout(this);
        panel.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));
        panel.setOrientation(LinearLayout.VERTICAL);

        mCountry = NavitAppConfig.sSettings.getString(("DefaultCountry"), null);

        if (mCountry == null) {
            Locale defaultLocale = Locale.getDefault();
            mCountry = defaultLocale.getCountry().toLowerCase(defaultLocale);
            SharedPreferences.Editor editSettings = NavitAppConfig.sSettings.edit();
            editSettings.putString("DefaultCountry", mCountry);
            editSettings.apply();
        }

        mCountryButton = new ImageButton(this);
        mCountryButton.setImageResource(getDrawableID("country_" + mCountry));
        mCountryButton.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                requestCountryDialog();
            }
        });

        // address: label and text field
        TextView addrView = new TextView(this);
        addrView.setText(NavitAppConfig.getTstring(
                "Enter cityname or postcode or select another country")); // TRANS
        addrView.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));
        addrView.setPadding(4, 4, 4, 4);

        final EditText addressString = new EditText(this);
        addressString.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);

        ListView zoekResults = new ListView(this);
        mAddressAdapter = new ArrayAdapter<>(this,
                android.R.layout.simple_list_item_1, mAddressesFound);

        zoekResults.setAdapter(mAddressAdapter);
        zoekResults.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view,
                    int position, long id) {
                if (mAddressesFound.get(position).mResultType < 2) {
                    callbackSearch(mSearchHandle, mZoektype,
                            mAddressesFound.get(position).mId, "");

                    if (mAddressesFound.get(position).mResultType == 0) {
                        mSelectedTown = mAddressesFound.get(position);
                        mResultActionButton.setText(mSelectedTown.toString());
                    }
                    if (mAddressesFound.get(position).mResultType == 1) {
                        mSelectedStreet = mAddressesFound.get(position);
                        mResultActionButton.setText(
                                mSelectedTown.toString() + ", " + mSelectedStreet.toString());
                    }
                    addressString.setText("");
                    //          if (zoektype == zoekTypeHouseNumber){
                    //              addressString.setInputType(InputType.TYPE_CLASS_NUMBER);
                    //          } else {
                    //              addressString.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                    //          }
                    mZoektype++;
                } else {
                    addressDialog(mAddressesFound.get(position));
                }
            }
        });

        mZoekBar = new ProgressBar(this.getApplicationContext());
        mZoekBar.setIndeterminate(true);
        mZoekBar.setVisibility(View.INVISIBLE);

        mResultActionButton = new Button(this);
        mResultActionButton.setText("test");
        mResultActionButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View arg0) {
                if (mSelectedStreet != null) {
                    addressDialog(mSelectedStreet);
                } else if (mSelectedTown != null) {
                    addressDialog(mSelectedTown);
                }
            }
        });

        final TextWatcher watcher = new TextWatcher() {
            @Override
            public void afterTextChanged(Editable addressEditString) {
                sAddressString = addressEditString.toString();
                mAddressAdapter.clear();
                if ((sAddressString.length() > 1) || (sAddressString.length() > 0 && !(mZoektype
                        == mZoekTypeTown))) {
                    mZoekBar.setVisibility(View.VISIBLE);
                    mOngoingSearches++;
                    search();
                } else { // voor geval een backspace is gebruikt
                    if (mSearchHandle != 0) {
                        callbackCancelAddressSearch(mSearchHandle);
                    }
                }
            }

            @Override
            public void beforeTextChanged(CharSequence arg0, int arg1,
                    int arg2, int arg3) {
                // Auto-generated method stub
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before,
                    int count) {
                // Auto-generated method stub
            }
        };

        addressString.addTextChangedListener(watcher);
        addressString.setSelectAllOnFocus(true);
        String title = NavitAppConfig.getTstring(R.string.address_search_title);

        if (title != null && title.length() > 0) {
            this.setTitle(title);
        }

        LinearLayout searchSettingsLayout = new LinearLayout(this);
        searchSettingsLayout.setOrientation(LinearLayout.HORIZONTAL);
        searchSettingsLayout.addView(mCountryButton);
        searchSettingsLayout.addView(mZoekBar);
        searchSettingsLayout.addView(mResultActionButton);

        panel.addView(addrView);
        panel.addView(addressString);
        panel.addView(searchSettingsLayout);
        panel.addView(zoekResults);

        setContentView(panel);
    }

    private void requestCountryDialog() {
        final String[][] allCountries = NavitGraphics.getAllCountries();
        Comparator<String[]> countryComperator = new Comparator<String[]>() {
            public int compare(String[] object1, String[] object2) {
                return object1[1].compareTo(object2[1]);
            }
        };

        Arrays.sort(allCountries, countryComperator);

        AlertDialog.Builder mapModeChooser = new AlertDialog.Builder(this);
        String[] countryName = new String[allCountries.length];

        for (int countryIndex = 0; countryIndex < allCountries.length; countryIndex++) {
            countryName[countryIndex] = allCountries[countryIndex][1];
        }

        mapModeChooser.setItems(countryName,
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int item) {
                        mCountry = allCountries[item][0];
                        SharedPreferences.Editor editSettings = NavitAppConfig.sSettings
                                .edit();
                        editSettings.putString("DefaultCountry", mCountry);
                        editSettings.apply();
                        mCountryButton
                                .setImageResource(getDrawableID("country_"
                                        + mCountry));
                    }
                });

        mapModeChooser.show();
    }




    private void addressDialog(NavitSearchAddress address) {

        final NavitSearchAddress addressSelected = address;
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(addressSelected.mAddr);
        builder.setPositiveButton("Als bestemming instellen",
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                              Intent resultIntent = new Intent();
                              resultIntent.putExtra("lat", addressSelected.mLat);
                              resultIntent.putExtra("lon", addressSelected.mLon);
                              resultIntent.putExtra("q", addressSelected.mAddr);
                              setResult(Activity.RESULT_OK, resultIntent);
                              finish();
                    }
                });
        builder.setNegativeButton("cancel",
                new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog,
                                        int id) {

                    }
                });

        builder.show();
    }

    /**
     * Receives found addresses from the navitve code.
     */
    @SuppressWarnings("unused")
    public void receiveAddress(int type, int id, float latitude, float longitude, String address, String extras) {

        mAddressesFound.insert(new NavitSearchAddress(type, id, latitude, longitude, address, extras));
        //mAddressAdapter.add(new NavitAddress(type, id, latitude, longitude, address,
        //        extras));

        // jdgAdapter.sort(new NavitAddressComparator());
        //
        mAddressAdapter.notifyDataSetChanged();
        // -> is voor live update
        // loopt veel soepeler zonder live update met sorted insert
        // de lijst maar tonen when complete helpt niet merkbaar

    }

    /*
    * This gets called from the native code to indicate
    * no more search results are following
    *
    * */
    @SuppressWarnings("unused")
    void finishAddressSearch() {
        // search_handle = 0;
        // versie update when complete

        // comparator zit nu in addresslist, als dit nog zou gebruikt worden moet er een
        // implement toegevoegd worden of zo
        // mAddressAdapter.sort(new NavitAddressComparator());
        mAddressAdapter.notifyDataSetChanged();
        Log.e(TAG, "ongoingSearches " + mOngoingSearches);
        if (mOngoingSearches > 0) {
            mOngoingSearches--;
        }
        if (mOngoingSearches == 0) {
            mZoekBar.setVisibility(View.INVISIBLE);
        }
        Log.d(TAG, "end of search");
    }

    public native long callbackStartAddressSearch(int partialMatch, String country, String s);

    public native void callbackCancelAddressSearch(long handle);

    public native void callbackSearch(long handle, int type, int id, String str);

    private void search() {
        if (mSearchHandle != 0 && mOngoingSearches > 1) {
            callbackCancelAddressSearch(mSearchHandle);
        }
        mAddressAdapter.clear();
        if (mSearchHandle == 0) {
            boolean mPartialMatch = true; // is a legacy param
            mSearchHandle = callbackStartAddressSearch(mPartialMatch ? 1 : 0,
                    mCountry, sAddressString);
        }
        callbackSearch(mSearchHandle, mZoektype, 0, sAddressString);
    }


    class NavitAddressList extends ArrayList<NavitSearchAddress> {

        private static final long serialVersionUID = 1L;

        void insert(NavitSearchAddress address) {
            //  NavitAddressComparator comp = new NavitAddressComparator();
            int index = this.size() - 1;
            if (index >= 0) {
                while (index >= 0 && compare(this.get(index), address) > 0) {
                    index--;
                }
                this.add(index + 1, address);
            } else {
                this.add(address);
            }
        }

        int compare(NavitSearchAddress lhs, NavitSearchAddress rhs) {

            if (lhs.mResultType == 2
                    && rhs.mResultType == 2) {
                String lhsNum = "";
                String rhsNum = "";
                if (lhs.mAddr.length() > 0) {
                    lhsNum = lhs.mAddr.split("[a-zA-Z]")[0];
                }

                if (rhs.mAddr.length() > 0) {
                    rhsNum = rhs.mAddr.split("[a-zA-Z]")[0];
                }

                if (lhsNum.length() < rhsNum.length()) {
                    return -1;
                }
                if (lhsNum.length() > rhsNum.length()) {
                    return 1;
                }
            }
            String lhsNormalized = Normalizer.normalize(lhs.mAddr, Normalizer.Form.NFD)
                    .replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase();
            String rhsNormalized = Normalizer.normalize(rhs.mAddr, Normalizer.Form.NFD)
                    .replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase();
            if (lhsNormalized.indexOf(sAddressString.toLowerCase()) == 0
                    && rhsNormalized.toLowerCase().indexOf(sAddressString.toLowerCase()) != 0) {
                return -1;
            }
            if (lhsNormalized.indexOf(sAddressString.toLowerCase()) != 0
                    && rhsNormalized.toLowerCase().indexOf(sAddressString.toLowerCase()) == 0) {
                return 1;
            }
            return (lhsNormalized.compareTo(rhsNormalized));
        }
    }
}

