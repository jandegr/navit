/**
 * Navit, a modular navigation system. Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

package org.navitproject.navit;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
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
import java.text.Normalizer.Form;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

public class NavitAddressSearchActivity extends Activity {

    private static final String TAG = "NavitAddress";
    private static final int resultTypeHouseNumber = 2;
    private static String mAddressString = "";
    // TODO remember settings
    private static String last_address_search_string = "";
    private ProgressBar zoekBar;
    private final int zoekTypeTown = 2; // in enum steken ?
    private int zoektype = zoekTypeTown; // town
    private int ongoingSearches = 0;
    private ArrayAdapter<NavitAddress> addressAdapter;
    private final List<NavitAddress> addressesFound = new NavitAddressList<>();
    private NavitAddress selectedTown;
    private NavitAddress selectedStreet;
    private boolean mPartialSearch = true;
    private String mCountry;
    private ImageButton mCountryButton;
    private Button resultActionButton;
    private long searchHandle = 0;

    private int getDrawableID(String resourceName) {
        int drawableId = 0;
        try {
            Class<?> res = R.drawable.class;
            Field field = res.getField(resourceName + "_64_64");
            drawableId = field.getInt(null);
        } catch (Exception e) {
            Log.e("NavitAddressSearch", "Failure to get drawable id.", e);
        }
        return drawableId;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // ACRA.getErrorReporter().setEnabled(true);
        // Bundle extras = getIntent().getExtras();
        // if ( extras != null )
        // {
        // String search_string = extras.getString(("search_string"));
        // if (search_string != null) {
        // mPartialSearch = true;
        // mAddressString = search_string;
        // executeSearch();
        // return;
        // }
        // }

        // mPartialSearch = last_address_partial_match;
        mAddressString = last_address_search_string;

        getWindow().setFlags(WindowManager.LayoutParams.FLAG_BLUR_BEHIND,
                WindowManager.LayoutParams.FLAG_BLUR_BEHIND);
        LinearLayout panel = new LinearLayout(this);
        panel.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));
        panel.setOrientation(LinearLayout.VERTICAL);

        // address: label and text field
        SharedPreferences settings = getSharedPreferences(Navit.NAVIT_PREFS,
                MODE_PRIVATE);
        mCountry = settings.getString(("DefaultCountry"), null);

        if (mCountry == null) {
            Locale defaultLocale = Locale.getDefault();
            mCountry = defaultLocale.getCountry().toLowerCase(defaultLocale);
            SharedPreferences.Editor editSettings = settings.edit();
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
        addrView.setText(Navit.navitTranslate(
                "Enter cityname or postcode or select another country")); // TRANS
        addrView.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));
        addrView.setPadding(4, 4, 4, 4);

        final EditText addressString = new EditText(this);
        addressString.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);

        ListView zoekResults = new ListView(this);
        addressAdapter = new ArrayAdapter<>(this,
                android.R.layout.simple_list_item_1, addressesFound);

        zoekResults.setAdapter(addressAdapter);
        zoekResults.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view,
                    int position, long id) {
                if (addressesFound.get(position).resultType < 2) {
                    CallbackSearch(searchHandle, zoektype,
                            addressesFound.get(position).id, "");

                    if (addressesFound.get(position).resultType == 0) {
                        selectedTown = addressesFound.get(position);
                        resultActionButton.setText(selectedTown.toString());
                    }
                    if (addressesFound.get(position).resultType == 1) {
                        selectedStreet = addressesFound.get(position);
                        resultActionButton.setText(
                                selectedTown.toString() + ", " + selectedStreet.toString());
                    }
                    addressString.setText("");
                    //          if (zoektype == zoekTypeHouseNumber){
                    //              addressString.setInputType(InputType.TYPE_CLASS_NUMBER);
                    //          } else {
                    //              addressString.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                    //          }
                    zoektype++;
                } else {
                    addressDialog(addressesFound.get(position));
                }
            }
        });

        zoekBar = new ProgressBar(this.getApplicationContext());
        zoekBar.setIndeterminate(true);
        zoekBar.setVisibility(View.INVISIBLE);

        resultActionButton = new Button(this);
        resultActionButton.setText("test");
        resultActionButton.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View arg0) {
                if (selectedStreet != null) {
                    addressDialog(selectedStreet);
                } else if (selectedTown != null) {
                    addressDialog(selectedTown);
                }
            }
        });

        final TextWatcher watcher = new TextWatcher() {
            @Override
            public void afterTextChanged(Editable addressEditString) {
                mAddressString = addressEditString.toString();
                addressAdapter.clear();
                if ((mAddressString.length() > 1) || (mAddressString.length() > 0 && !(zoektype
                        == zoekTypeTown))) {
                    zoekBar.setVisibility(View.VISIBLE);
                    ongoingSearches++;
                    search();
                } else { // voor geval een backspace is gebruikt
                    if (searchHandle != 0) {
                        CallbackCancelAddressSearch(searchHandle);
                    }
                }
            }

            @Override
            public void beforeTextChanged(CharSequence arg0, int arg1,
                    int arg2, int arg3) {
                // TODO Auto-generated method stub
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before,
                    int count) {
                // TODO Auto-generated method stub
            }
        };

        addressString.addTextChangedListener(watcher);
        addressString.setSelectAllOnFocus(true);
        String title = Navit.getInstance().getTstring(R.string.address_search_title);

        if (title != null && title.length() > 0) {
            this.setTitle(title);
        }

        LinearLayout searchSettingsLayout = new LinearLayout(this);
        searchSettingsLayout.setOrientation(LinearLayout.HORIZONTAL);
        searchSettingsLayout.addView(mCountryButton);
        searchSettingsLayout.addView(zoekBar);
        searchSettingsLayout.addView(resultActionButton);

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
                        SharedPreferences settings = getSharedPreferences(
                                Navit.NAVIT_PREFS, MODE_PRIVATE);
                        mCountry = allCountries[item][0];
                        SharedPreferences.Editor editSettings = settings
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

    private void addressDialog(NavitAddress address) {

        final NavitAddress addressSelected = address;

        class AddressDialogFragment extends DialogFragment {

            @Override
            public Dialog onCreateDialog(Bundle savedInstanceState) {

                // Use the Builder class for convenient dialog construction
                AlertDialog.Builder builder = new AlertDialog.Builder(
                        getActivity());
                builder.setMessage("address " + addressSelected.addr)
                        .setPositiveButton("Als bestemming instellen",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                            int id) {

                                        Intent resultIntent = new Intent();

                                        resultIntent.putExtra("lat",
                                                addressSelected.lat);
                                        resultIntent.putExtra("lon",
                                                addressSelected.lon);
                                        resultIntent.putExtra("q",
                                                addressSelected.addr);

                                        setResult(Activity.RESULT_OK,
                                                resultIntent);
                                        finish();
                                    }
                                })
                        //  .setPositiveButton("show on map",
                        //          new DialogInterface.OnClickListener() {
                        //              public void onClick(DialogInterface dialog,
                        //                      int id) {
                        //                  Message msg = Message.obtain(Navit.N_NavitGraphics.callback_handler,
                        //                  NavitGraphics.msg_type.CLB_CALL_CMD.ordinal());
                        //                  Bundle b = new Bundle();
                        //                  String command = "set_center(\"" + addressSelected.lon + " "
                        //                  + addressSelected.lat + "\");";
                        //                  b.putString("cmd", command);
                        //                  msg.setData(b);
                        //                  msg.sendToTarget();
                        //              }
                        //          })
                        .setNegativeButton("cancel",
                                new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,
                                            int id) {
                                        // User cancelled the dialog
                                        AddressDialogFragment.this.getDialog()
                                                .cancel();
                                    }
                                });
                // Create the AlertDialog object and return it
                return builder.create();
            }
        }
        AddressDialogFragment newFragment = new AddressDialogFragment();
        newFragment.show(getFragmentManager(), "missiles");
    }

    /**
     * start a search on the map.
     */
    public void receiveAddress(int type, int id, float latitude,
            float longitude, String address, String extras) {

        // -> ((NavitAddressList<NavitAddress>) Addresses_found).insert(new
        // NavitAddress(type, latitude, longitude,0,0, address));
        addressAdapter.add(new NavitAddress(type, id, latitude, longitude, address,
                extras));

        // jdgAdapter.sort(new NavitAddressComparator());
        //
        // -> jdgAdapter.notifyDataSetChanged();
        // -> is voor live update
        // loopt veel soepeler zonder live update met sorted insert
        // de lijst maar tonen when complete helpt niet merkbaar

    }

    // wordt aangeroepen vanuit de native code
    public void finishAddressSearch() {
        // search_handle = 0;
        // versie update when complete
        addressAdapter.sort(new NavitAddressComparator());
        addressAdapter.notifyDataSetChanged();
        Log.e(TAG, "ongoingSearches " + ongoingSearches);
        if (ongoingSearches > 0) {
            ongoingSearches--;
        }
        if (ongoingSearches == 0) {
            zoekBar.setVisibility(View.INVISIBLE);
        }
        Log.d(TAG, "einde zoeken");
    }

    public native long CallbackStartAddressSearch(int partialMatch, String country, String s);

    public native void CallbackCancelAddressSearch(long handle);

    public native void CallbackSearch(long handle, int type, int id, String str);

    private void search() {
        if (searchHandle != 0 && ongoingSearches > 1) {
            CallbackCancelAddressSearch(searchHandle);
        }
        addressAdapter.clear();
        if (searchHandle == 0) {
            searchHandle = CallbackStartAddressSearch(mPartialSearch ? 1 : 0,
                    mCountry, mAddressString);
        }
        CallbackSearch(searchHandle, zoektype, 0, mAddressString);
    }

    public static final class NavitAddress {

        int resultType;
        int id;
        float lat;
        float lon;
        String addr;
        String addrExtras;

        NavitAddress(int type, int id, float latitude, float longitude,
                String address, String addrExtras) {
            resultType = type;
            this.id = id;
            lat = latitude;
            lon = longitude;
            addr = address;
            this.addrExtras = addrExtras;
        }

        public String toString() {
            if (resultType == 2) { // huisnummer
                return this.addr;
            }
            if (resultType == 1) { // straat
                return this.addr;
            }
            return (this.addr + " " + this.addrExtras);
        }
    }

    static class NavitAddressComparator implements
            Comparator<NavitAddress> {

        //      @Override
        public int compare(NavitAddress lhs, NavitAddress rhs) {

            if (lhs.resultType == resultTypeHouseNumber
                    && rhs.resultType == resultTypeHouseNumber) {
                String lhsNum = "";
                String rhsNum = "";
                if (lhs.addr.length() > 0) {
                    lhsNum = lhs.addr.split("[a-zA-Z]")[0];
                }

                if (rhs.addr.length() > 0) {
                    rhsNum = rhs.addr.split("[a-zA-Z]")[0];
                }

                if (lhsNum.length() < rhsNum.length()) {
                    return -1;
                }
                if (lhsNum.length() > rhsNum.length()) {
                    return 1;
                }
            }
            String lhsNormalized = Normalizer.normalize(lhs.addr, Form.NFD)
                    .replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase();
            String rhsNormalized = Normalizer.normalize(rhs.addr, Form.NFD)
                    .replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase();
            if (lhsNormalized.indexOf(mAddressString.toLowerCase()) == 0
                    && rhsNormalized.toLowerCase().indexOf(mAddressString.toLowerCase()) != 0) {
                return -1;
            }
            if (lhsNormalized.indexOf(mAddressString.toLowerCase()) != 0
                    && rhsNormalized.toLowerCase().indexOf(mAddressString.toLowerCase()) == 0) {
                return 1;
            }
            return (lhsNormalized.compareTo(rhsNormalized));
        }
    }

    public class NavitAddressList<T> extends ArrayList<T> {

        private static final long serialVersionUID = 1L;


        public void insert(NavitAddress address) {
            NavitAddressComparator comp = new NavitAddressComparator();
            int index = this.size() - 1;
            if (index >= 0) {
                while (index >= 0 && comp.compare((NavitAddress) this.get(index), address) > 0) {
                    index--;
                }
                this.add(index + 1, (T) address);
            } else {
                this.add((T) address);
            }
        }
    }
}



