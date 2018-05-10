/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

package org.navitproject.navit;

import java.lang.reflect.Field;
import java.text.Normalizer;
import java.text.Normalizer.Form;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
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
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;

public class NavitAddressSearchActivity extends Activity {
	public static final class NavitAddress {
		int resultType;
		int id;
		float lat;
		float lon;
		String addr;
		String addrExtras;

		public NavitAddress(int type, int id, float latitude, float longitude,
				String address, String addrExtras) {
			resultType = type;
			this.id = id;
			lat = latitude;
			lon = longitude;
			addr = address;
			this.addrExtras = addrExtras;
		}

		public String toString() {
			if (resultType == 2) // huisnummer
				return this.addr;
			if (resultType == 1) // straat
				return this.addr;
			return (this.addr + " " + this.addrExtras);
		}
	}

	public static class NavitAddressComparator implements
			Comparator<NavitAddress> {

//		@Override
		public int compare(NavitAddress lhs, NavitAddress rhs) {
			
			if (lhs.resultType == resultTypeHouseNumber && rhs.resultType == resultTypeHouseNumber) {
				String lhsNum = "";
				String rhsNum = "";
				if (lhs.addr.length() > 0)
					lhsNum = lhs.addr.split("[a-zA-Z]")[0];

				if (rhs.addr.length() > 0)
					rhsNum = rhs.addr.split("[a-zA-Z]")[0];

				if (lhsNum.length() < rhsNum.length())
					return -1;
				if (lhsNum.length() > rhsNum.length())
					return 1;
			}
			String lhsNormalized = Normalizer.normalize(lhs.addr,Form.NFD)
					.replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase(); 
			String rhsNormalized = Normalizer.normalize(rhs.addr,Form.NFD)
					.replaceAll("\\p{InCombiningDiacriticalMarks}+", "").toLowerCase();
			if (lhsNormalized.indexOf(mAddressString.toLowerCase()) == 0
						&& rhsNormalized.toLowerCase().indexOf(mAddressString.toLowerCase()) != 0)
				return -1;
			if (lhsNormalized.indexOf(mAddressString.toLowerCase()) != 0 
						&& rhsNormalized.toLowerCase().indexOf(mAddressString.toLowerCase()) == 0)
				return 1;
			return (lhsNormalized.compareTo(rhsNormalized));
		}
	}
	public class NavitAddressList<T> extends ArrayList<T> {

		private static final long serialVersionUID = 1L;

		@SuppressWarnings("unchecked")
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

	private static final String TAG = "NavitAddress";
	private static final int ADDRESS_RESULT_PROGRESS_MAX = 10;

	private List<NavitAddress> Addresses_found = new NavitAddressList<NavitAddress>();;
	private List<NavitAddress> addresses_shown = null;
	private NavitAddress selectedTown;
	private NavitAddress selectedStreet;
	private static String mAddressString = "";
	private boolean mPartialSearch = true;
	private String mCountry;
	private ImageButton mCountryButton;
	private Button resultActionButton ;
	ProgressBar zoekBar;
	ProgressDialog search_results_wait = null;
	public RelativeLayout NavitAddressSearchActivity_layout;
	private int search_results_towns = 0;
	private int search_results_streets = 0;
	private int search_results_streets_hn = 0;
	private long search_handle = 0;
	int zoekTypeTown = 2; // in enum steken ?
	int zoekTypeHouseNumber = 3;
	int zoektype = zoekTypeTown; // town
	static int resultTypeHouseNumber = 2;
	int ongoingSearches = 0;

	// TODO remember settings
	private static String last_address_search_string = "";
	private static Boolean last_address_partial_match = false;
	private static String last_country = "";

	ArrayAdapter<NavitAddress> addressAdapter;

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
//		ACRA.getErrorReporter().setEnabled(true);
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
			SharedPreferences.Editor edit_settings = settings.edit();
			edit_settings.putString("DefaultCountry", mCountry);
			edit_settings.commit();
		}

		mCountryButton = new ImageButton(this);
		mCountryButton.setImageResource(getDrawableID("country_" + mCountry));
		mCountryButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				requestCountryDialog();
			}
		});

		// address: label and text field
		TextView addr_view = new TextView(this);
		addr_view.setText(Navit._("Enter city or select another country")); // TRANS
		addr_view.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
				LayoutParams.WRAP_CONTENT));
		addr_view.setPadding(4, 4, 4, 4);

		final EditText address_string = new EditText(this);
		address_string.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);

		ListView zoekResults = new ListView(this);
		addressAdapter = new ArrayAdapter<NavitAddress>(this,
				android.R.layout.simple_list_item_1, Addresses_found);

		zoekResults.setAdapter(addressAdapter);
		zoekResults.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> parent, View view,
					int position, long id) {
				if (Addresses_found.get(position).resultType < 2) {
					CallbackSearch(search_handle, zoektype,
							Addresses_found.get(position).id, "");
					
					if (Addresses_found.get(position).resultType == 0){
						selectedTown = Addresses_found.get(position);
						resultActionButton.setText(selectedTown.toString());
					}
					if (Addresses_found.get(position).resultType == 1){
						selectedStreet = Addresses_found.get(position);	
						resultActionButton.setText(selectedTown.toString() + ", " + selectedStreet.toString());
					}
					address_string.setText("");
		//			if (zoektype == zoekTypeHouseNumber){
		//				address_string.setInputType(InputType.TYPE_CLASS_NUMBER);
		//			} else {
		//				address_string.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
		//			}
					zoektype++;
				} else {
					addressDialog(Addresses_found.get(position));
				}
			}
		});

		zoekBar = new ProgressBar(this.getApplicationContext());
		zoekBar.setIndeterminate(true);
		zoekBar.setVisibility(View.INVISIBLE);
		
		resultActionButton = new Button(this);
		resultActionButton.setText("test");
		resultActionButton.setOnClickListener(new OnClickListener(){

		@Override
		public void onClick(View arg0) {
			if (selectedStreet != null)
				addressDialog(selectedStreet);
			else if (selectedTown != null)
				addressDialog(selectedTown);		
		}});

		final TextWatcher watcher = new TextWatcher() {
			@Override
			public void afterTextChanged(Editable address_edit_string) {
				mAddressString = address_edit_string.toString();
				addressAdapter.clear();
				if ((mAddressString.length() > 1 ) || (mAddressString.length() > 0 && !(zoektype == zoekTypeTown))) {
					zoekBar.setVisibility(View.VISIBLE);
					ongoingSearches++;
					search();
				} else { // voor geval een backspace is gebruikt
					if (search_handle != 0)
						CallbackCancelAddressSearch(search_handle);
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
		
		address_string.addTextChangedListener(watcher);

		address_string.setSelectAllOnFocus(true);

		NavitAppConfig navitConfig = (NavitAppConfig) getApplicationContext();

		String title = getString(R.string.address_search_title);

		if (title != null && title.length() > 0)
			this.setTitle(title);

		LinearLayout searchSettingsLayout = new LinearLayout(this);
		searchSettingsLayout.setOrientation(LinearLayout.HORIZONTAL);
		searchSettingsLayout.addView(mCountryButton);
		searchSettingsLayout.addView(zoekBar);
		searchSettingsLayout.addView(resultActionButton);

		panel.addView(addr_view);
		panel.addView(address_string);
		panel.addView(searchSettingsLayout);
		panel.addView(zoekResults);

		setContentView(panel);
	}

	private void requestCountryDialog() {
		final String[][] all_countries = NavitGraphics.GetAllCountries();
		Comparator<String[]> country_comperator = new Comparator<String[]>() {
			public int compare(String[] object1, String[] object2) {
				return object1[1].compareTo(object2[1]);
			}
		};

		Arrays.sort(all_countries, country_comperator);

		AlertDialog.Builder mapModeChooser = new AlertDialog.Builder(this);
		String[] country_name = new String[all_countries.length];

		for (int country_index = 0; country_index < all_countries.length; country_index++) {
			country_name[country_index] = all_countries[country_index][1];
		}

		mapModeChooser.setItems(country_name,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int item) {
						SharedPreferences settings = getSharedPreferences(
								Navit.NAVIT_PREFS, MODE_PRIVATE);
						mCountry = all_countries[item][0];
						SharedPreferences.Editor edit_settings = settings
								.edit();
						edit_settings.putString("DefaultCountry", mCountry);
						edit_settings.commit();
						mCountryButton
								.setImageResource(getDrawableID("country_"
										+ mCountry));
					}
				});

		mapModeChooser.show();
	}

	public void addressDialog(NavitAddress address) {

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
					//	.setPositiveButton("show on map",
					//			new DialogInterface.OnClickListener() {
					//				public void onClick(DialogInterface dialog,
					//						int id) {						
					//					Message msg = Message.obtain(Navit.N_NavitGraphics.callback_handler, NavitGraphics.msg_type.CLB_CALL_CMD.ordinal());
					//					Bundle b = new Bundle();
					//					String command = "set_center(\"" + addressSelected.lon + " " + addressSelected.lat + "\");";
					//					b.putString("cmd", command);
					//					msg.setData(b);
					//					msg.sendToTarget();
					//				}
					//			})
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
	 * start a search on the map
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

	public native long CallbackStartAddressSearch(int partial_match, String country, String s);

	public native void CallbackCancelAddressSearch(long handle);

	public native void CallbackSearch(long handle, int type, int id, String s);

	void search() {
		if (search_handle != 0 && ongoingSearches > 1)
			CallbackCancelAddressSearch(search_handle);
		addressAdapter.clear();
		search_results_towns = 0;
		search_results_streets = 0;
		search_results_streets_hn = 0;
		if (search_handle == 0)
			search_handle = CallbackStartAddressSearch(mPartialSearch ? 1 : 0,
					mCountry, mAddressString);
		CallbackSearch(search_handle, zoektype, 0, mAddressString);
	}
}



