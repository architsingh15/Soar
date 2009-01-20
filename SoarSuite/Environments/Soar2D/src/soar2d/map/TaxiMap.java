package soar2d.map;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;

import org.apache.log4j.Logger;

import soar2d.Direction;
import soar2d.Names;
import soar2d.Simulation;
import soar2d.Soar2D;
import soar2d.world.TankSoarWorld;

public class TaxiMap extends GridMap {
	private static Logger logger = Logger.getLogger(TaxiMap.class);

	// this should be a property of the player, but this is easier
	int fuel;
	
	public TaxiMap() {
		fuel = Simulation.random.nextInt(1 + Soar2D.config.taxiConfig().fuel_starting_maximum - Soar2D.config.taxiConfig().fuel_starting_minimum ) 
				+ Soar2D.config.taxiConfig().fuel_starting_minimum; 
	}
	
	public void consumeFuel() {
		if (Soar2D.config.taxiConfig().disable_fuel) {
			logger.info(Names.Info.fuelConsumption);
		}
		logger.info(Names.Info.fuel + Integer.toString(fuel) + " -> " + Integer.toString(fuel-1));
		fuel -= 1;
	}
	
	// passenger section
	CellObject passenger;
	int [] passengerLocation;
	int [] passengerDefaultLocation;
	String passengerDestination;
	String passengerDefaultDestination;
	String passengerFormerDestination;
	
	public String getPassengerFormerDestination() {
		return passengerFormerDestination;
	}

	public void setPassengerDefaults() {
		assert passenger == null;
		if (this.passengerLocation != null) {
			this.passengerDefaultLocation = Arrays.copyOf(this.passengerLocation, this.passengerLocation.length);
			this.passenger = this.getObject(passengerLocation, "passenger");
		} else {
			this.passenger = cellObjectManager.createObject("passenger");
		}
		
		if (this.passenger.hasProperty("passenger-destination")) {
			this.passengerDefaultDestination = this.passenger.getProperty("passenger-destination");
		}
	}
	
	public String getPassengerDestination() {
		assert passengerDestination != null;
		return passengerDestination;
	}
	private void setPassengerDestination() {
		passengerFormerDestination = destinationMap.get(passengerLocation);
		
		if (passengerDefaultDestination != null) {
			passengerDestination = passengerDefaultDestination;
		} else {
			
			int pick = Simulation.random.nextInt(destinationMap.values().size());
			Iterator<String> iter = destinationMap.values().iterator();
			for (int index = 0; index < pick; index++) {
				assert iter.hasNext();
				iter.next();
			}
			assert iter.hasNext();
			passengerDestination = iter.next();
		}
		
		logger.info("passenger destination: " + passengerDestination);
	}
	
	public boolean pickUp(int [] location) {
		if (passengerLocation == null) {
			return false;
		}
		if (location.equals(passengerLocation)) {
			CellObject obj = removeObject(location, "passenger");
			assert obj != null;
			passengerLocation = null;
			return true;
		}
		return false;
	}
	public boolean putDown(int [] location) {
		if (passengerLocation != null) {
			return false;
		}
		addObjectToCell(location, passenger);
		passengerFormerDestination = destinationMap.get(location);
		return true;
	}
	public boolean isPassengerCarried() {
		return passengerLocation == null;
	}
	
	public void placePassengerAndSetDestination() {
		assert passenger != null;
		
		if (passengerLocation != null) {
			if (passengerLocation.equals(passengerDefaultLocation)) {
				setPassengerDestination();
				return;
			}
			
			boolean ret = pickUp(passengerLocation);
			assert ret;
		}
		
		Collection<int [] > locations = destinationLocations.values();
		assert locations.size() > 1;
		
		int pick = Simulation.random.nextInt(locations.size());
		Iterator<int [] > iter = locations.iterator();
		for (int index = 0; index < pick; index++) {
			assert iter.hasNext();
			iter.next();
		}
		assert iter.hasNext();
		passengerLocation = iter.next();
		addObjectToCell(passengerLocation, passenger);
		
		setPassengerDestination();
	}

	public boolean isPassengerDestination(int []  location) {
		if (passengerDestination.equals(destinationMap.get(location))) {
			return true;
		}
		return false;
	}

	boolean passengerDelivered = false;
	public boolean isPassengerDelivered() {
		return passengerDelivered;
	}
	public void deliverPassenger() {
		passengerDelivered = true;
	}

	// end passenger section
	
	HashMap<CellObject, int []> destinationLocations = new HashMap<CellObject, int []>();
	HashMap<int [], String> destinationMap = new HashMap<int [], String>();
	
	@Override
	public void addObjectToCell(int [] location, CellObject object) {
		Cell cell = getCell(location);
		if (cell.hasObject(object.getName())) {
			CellObject old = cell.removeObject(object.getName());
			assert old != null;
			updatables.remove(old);
			updatablesLocations.remove(old);
			removalStateUpdate(old);
		}
		if (object.updatable()) {
			updatables.add(object);
			updatablesLocations.put(object, location);
		}
		
		if (object.hasProperty("destination")) {
			destinationLocations.put(object, location);
			destinationMap.put(location, object.getProperty("color"));
		}
		
		if (object.hasProperty("passenger")) {
			this.passengerLocation = Arrays.copyOf(location, location.length);
		}
		
		cell.addCellObject(object);
		setRedraw(cell);
	}
	
	@Override
	public boolean isAvailable(int []  location) {
		Cell cell = getCell(location);
		boolean destination = cell.getAllWithProperty("destination").size() > 0;
		boolean fuel = cell.hasObject("fuel");
		boolean noPlayer = cell.getPlayer() == null;
		return !destination && !fuel && noPlayer;
	}

	@Override
	void removalStateUpdate(CellObject object) {
		if (object.hasProperty("destination")) {
			int []  location = destinationLocations.remove(object);
			destinationMap.remove(location);
		}
	}

	@Override
	public void updateObjects(TankSoarWorld tsWorld) {
		
	}

	public boolean isFuelNegative() {
		return fuel < 0;
	}
	
	public int getFuel() {
		return fuel;
	}

	public boolean fillUp(int []  location) {
		CellObject fuelObject = getObject(location, "fuel");
		if (fuelObject == null) {
			return false;
		}
		int maximum = Soar2D.config.taxiConfig().fuel_maximum;
		logger.info("fuel: " + Integer.toString(fuel) + " -> " + maximum + " (fillup)");
		fuel = maximum;
		return true;
	}

	public String getStringType(int []  location) {
		if (!this.isInBounds(location)) {
			return "none";
		}
		
		ArrayList<CellObject> dests = this.getAllWithProperty(location, "destination");
		assert dests.size() < 2;
		if (dests.size() > 0) {
			return dests.get(0).getProperty("color");
		}
		
		if (this.getObject(location, "fuel") != null) {
			return "fuel";
		}
		
		return "normal";
	}
	
	public boolean wall(int []  from, int to) {
		Iterator<CellObject> iter = this.getAllWithProperty(from, "block").iterator();
		while (iter.hasNext()) {
			CellObject wall = iter.next();
			if (wall.getProperty("direction").equals(Direction.stringOf[to])) {
				return true;
			}
		}
		return false;
	}
}
