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
  const [humidity, setHumidity] = useState('--');
  const [temperature, setTemperature] = useState('--');
  const [lastData, setLastData] = useState({ humidity: null, temperature: null });

  const isDataFresh = (timestamp) => {
    if (!timestamp) return false;
    const now = new Date();
    const [hours, minutes, seconds] = timestamp.split(':').map(Number);
    const dataTime = new Date();
    dataTime.setHours(hours, minutes, seconds);
    
    const timeDiff = Math.abs(now.getTime() - dataTime.getTime());
    return timeDiff <= 10000; // 10 seconds
  };

  // Effect for Firebase listeners
  useEffect(() => {
    const db = getDatabase(app);
    const humidityRef = ref(db, 'humidity');
    const temperatureRef = ref(db, 'temperature');

    const humidityListener = onValue(humidityRef, (snapshot) => {
      const data = snapshot.val();
      setLastData(prev => ({ ...prev, humidity: data }));
    });

    const temperatureListener = onValue(temperatureRef, (snapshot) => {
      const data = snapshot.val();
      setLastData(prev => ({ ...prev, temperature: data }));
    });

    return () => {
      humidityListener();
      temperatureListener();
    };
  }, []);

  // Effect for periodic checks
  useEffect(() => {
    const updateValues = () => {
      if (lastData.humidity) {
        if (isDataFresh(lastData.humidity.timestamp)) {
          setHumidity(lastData.humidity.value);
        } else {
          setHumidity('--');
        }
      }

      if (lastData.temperature) {
        if (isDataFresh(lastData.temperature.timestamp)) {
          setTemperature(lastData.temperature.value);
        } else {
          setTemperature('--');
        }
      }
    };

    // Initial update
    updateValues();

    // Set up interval for periodic updates
    const interval = setInterval(updateValues, 1000);

    return () => clearInterval(interval);
  }, [lastData]);

  const hum_emoji = humidity === '--' ? '❓' : 
                   humidity <= 30 ? '🌵' : 
                   humidity > 60 ? '💧' : '😊';

  const temp_emoji = temperature === '--' ? '❓' : 
                    temperature <= 10 ? '❄️' : 
                    temperature > 30 ? '🔥' : '🌞';

  let fun = "ayyo saadhanam kaanunillalo";
  let msg = "";
  if (temperature === '--' || humidity === '--') {
    msg = fun;
  }
  else {
    msg = "";
  }
  return (
    <div className='data'>
      <ul className='data-list'>
        <li>
          <h2>Humidity</h2>
          <p>{humidity !== '--' ? `${humidity}%` : 'No data'} {hum_emoji}</p>
        </li>
        <li>
          <h2>Temperature</h2>
          <p>{temperature !== '--' ? `${temperature}°C` : 'No data'} {temp_emoji}</p>
        </li>
      </ul>
      <h2 id='msg'>{msg}</h2>
    </div>
  );
}

export default Data;
