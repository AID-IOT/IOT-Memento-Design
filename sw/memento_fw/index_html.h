/*
 * The control page, as one PROGMEM string.
 *
 * This lives in a header rather than in the .ino on purpose: the Arduino
 * preprocessor that generates function prototypes does not understand C++11
 * raw string literals, and tries to compile the JavaScript inside as C++
 * ("'function' does not name a type"). Headers are passed to the compiler
 * untouched, so the raw string survives.
 */
#ifndef MEMENTO_INDEX_HTML_H
#define MEMENTO_INDEX_HTML_H

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>memento</title>
<style>
:root{--bg:#0f1216;--card:#171c22;--line:#262d36;--fg:#e8edf2;--dim:#93a1b0;--ac:#4aa3ff}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);
  font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:720px;margin:0 auto}
h1{font-size:19px;margin:0 0 2px}
.sub{color:var(--dim);font-size:13px;margin-bottom:18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;
  padding:16px;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.stat{background:#10151a;border:1px solid var(--line);border-radius:10px;padding:14px}
.k{color:var(--dim);font-size:12px;text-transform:uppercase;letter-spacing:.06em}
.v{font-size:26px;font-weight:600;margin-top:4px;font-variant-numeric:tabular-nums}
.u{font-size:14px;color:var(--dim);font-weight:400}
h2{font-size:13px;color:var(--dim);text-transform:uppercase;letter-spacing:.06em;
  margin:0 0 12px;font-weight:600}
.row{display:flex;align-items:center;gap:10px;margin-bottom:12px;flex-wrap:wrap}
.row label{width:84px;color:var(--dim);font-size:13px}
button{background:#1e242c;color:var(--fg);border:1px solid var(--line);
  border-radius:8px;padding:8px 13px;cursor:pointer;font-size:13px}
button.on{background:var(--ac);border-color:var(--ac);color:#04111f;font-weight:600}
input[type=range]{flex:1;min-width:120px;accent-color:var(--ac)}
input[type=color]{width:46px;height:32px;padding:0;border:1px solid var(--line);
  border-radius:8px;background:none;cursor:pointer}
.num{color:var(--dim);font-size:13px;min-width:38px;text-align:right;
  font-variant-numeric:tabular-nums}
.warn{background:#2a1d15;border-color:#5a3a1f;color:#ffc78a}
.foot{color:var(--dim);font-size:12px;text-align:center;margin-top:18px}
</style></head><body><div class="wrap">
<h1>memento</h1>
<div class="sub">AICTE IDEA Lab &middot; Poornima College of Engineering</div>

<div id="sensorCard" class="card">
  <h2>BMP280</h2>
  <div class="grid">
    <div class="stat"><div class="k">Temperature</div>
      <div class="v"><span id="t">--</span><span class="u"> &deg;C</span></div></div>
    <div class="stat"><div class="k">Pressure</div>
      <div class="v"><span id="p">--</span><span class="u"> hPa</span></div></div>
    <div class="stat"><div class="k">Altitude</div>
      <div class="v"><span id="a">--</span><span class="u"> m</span></div></div>
  </div>
</div>

<div class="card">
  <h2>WS2812B &times; 4</h2>
  <div class="row" id="modes">
    <button data-m="solid">Solid</button>
    <button data-m="rainbow">Rainbow</button>
    <button data-m="breathe">Breathe</button>
    <button data-m="chase">Chase</button>
    <button data-m="off">Off</button>
  </div>
  <div class="row"><label for="col">Colour</label>
    <input type="color" id="col" value="#0078ff"></div>
  <div class="row"><label for="bri">Brightness</label>
    <input type="range" id="bri" min="0" max="255" value="60">
    <span class="num" id="briV">60</span></div>
  <div class="row"><label for="spd">Speed</label>
    <input type="range" id="spd" min="5" max="120" value="25">
    <span class="num" id="spdV">25</span></div>
</div>

<div class="foot" id="foot">connecting&hellip;</div>
</div><script>
var $=function(i){return document.getElementById(i);};
var mode="rainbow",busy=false;

function paintModes(){
  var bs=document.querySelectorAll('#modes button');
  for(var i=0;i<bs.length;i++){
    bs[i].className=(bs[i].getAttribute('data-m')===mode)?'on':'';
  }
}
function hex2rgb(h){
  return [parseInt(h.substr(1,2),16),parseInt(h.substr(3,2),16),
          parseInt(h.substr(5,2),16)];
}
function rgb2hex(r,g,b){
  function p(v){var s=v.toString(16);return s.length<2?'0'+s:s;}
  return '#'+p(r)+p(g)+p(b);
}

function push(){
  if(busy)return;
  busy=true;
  var c=hex2rgb($('col').value);
  var q='mode='+mode+'&r='+c[0]+'&g='+c[1]+'&b='+c[2]+
        '&brightness='+$('bri').value+'&speed='+$('spd').value;
  fetch('/api/led?'+q).catch(function(){}).then(function(){busy=false;});
}

function poll(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(s){
    if(s.sensor.ok){
      $('t').textContent=s.sensor.temperature_c.toFixed(2);
      $('p').textContent=s.sensor.pressure_hpa.toFixed(2);
      $('a').textContent=s.sensor.altitude_m.toFixed(1);
      $('sensorCard').className='card';
    }else{
      $('t').textContent='--';$('p').textContent='--';$('a').textContent='--';
      $('sensorCard').className='card warn';
    }
    var up=s.uptime_s;
    var txt=s.net.mode+' · '+s.net.ip;
    if(s.net.rssi){txt+=' · '+s.net.rssi+' dBm';}
    txt+=' · up '+Math.floor(up/60)+'m '+(up%60)+'s';
    txt+=s.sensor.ok?(' · BMP280 @ 0x'+s.sensor.address.toString(16))
                    :' · BMP280 not detected';
    $('foot').textContent=txt;
  }).catch(function(){
    $('foot').textContent='lost connection to board';
  });
}

function init(){
  var bs=document.querySelectorAll('#modes button');
  for(var i=0;i<bs.length;i++){
    bs[i].onclick=function(){
      mode=this.getAttribute('data-m');paintModes();push();
    };
  }
  $('bri').oninput=function(){$('briV').textContent=$('bri').value;push();};
  $('spd').oninput=function(){$('spdV').textContent=$('spd').value;push();};
  $('col').oninput=push;
  fetch('/api/status').then(function(r){return r.json();}).then(function(s){
    mode=s.led.mode;paintModes();
    $('col').value=rgb2hex(s.led.r,s.led.g,s.led.b);
    $('bri').value=s.led.brightness;$('briV').textContent=s.led.brightness;
    $('spd').value=s.led.speed;$('spdV').textContent=s.led.speed;
  }).catch(function(){});
  paintModes();poll();setInterval(poll,2000);
}
init();
</script></body></html>)HTMLPAGE";

#endif  // MEMENTO_INDEX_HTML_H
