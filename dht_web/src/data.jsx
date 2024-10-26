import React, { useState, useEffect } from 'react';
import { initializeApp } from 'firebase/app';
import { getDatabase, ref, onValue } from 'firebase/database';
import './data.css';

const firebaseConfig = {
  apiKey: "AIzaSyBlWsZE8PsCpOWn1vpA4EE9Ye__dP12NQo",
  databaseURL: "https://weather-81834-default-rtdb.asia-southeast1.firebasedatabase.app/",
};

const app = initializeApp(firebaseConfig);

function Data() {
  const [humidity, setHumidity] = useState(null);
  const [temperature, setTemperature] = useState(null);

  useEffect(() => {
    const db = getDatabase(app);
    const humidityRef = ref(db, 'humidity/value');
    const temperatureRef = ref(db, 'temperature/value');

    const humidityListener = onValue(humidityRef, (snapshot) => {
      const data = snapshot.val();
      setHumidity(data);
    });

    const temperatureListener = onValue(temperatureRef, (snapshot) => {
      const data = snapshot.val();
      setTemperature(data);
    });

    return () => {
      humidityListener();
      temperatureListener();
    };
  }, []); 
  const hum_emoji = humidity <= 30 ? '🌵' : humidity > 60 ? '💧' : '😊';
  const temp_emoji = temperature <= 10 ? '❄️' : temperature > 30 ? '🔥' : '🌞';
  return (
    <div className='data'>
        <ul className='data-list'>
            <li><h2>Humidity</h2>
            <p>{humidity}% {hum_emoji}</p></li>
            <li><h2>Temperature</h2>
            <p>{temperature}°C {temp_emoji}</p></li>
        </ul>
      {/* {humidity !== null && (
        <div>
          
        </div>
      )}
      {temperature !== null && (
        <div>
          <h2>Temperature</h2>
          <p>Value: {temperature}°C</p>
        </div>
      )} */}
    </div>
  );
}

export default Data;
