boolean gprs_send() 
{
  String request = "GET /cgi-bin/parnik2_upd?ts="; 
  unsigned int len;
  if (!useGPRS) return false;
  request += rp->ts;
  request += "&T=";
  request += rp->temp1;
  request += ":";
  request += rp->temp2;
  request += ":";
  request += rp->temp3;
  request += ":U:U:";
  request += sp->temp_fans;
  request += ":";
  request += sp->temp_pump;
  request += "&M=";
  request += rp->fans;
  request += ":";
  request += rp->pump;
  request += "&P=";
  request += rp->volt;
  request += ":U&V=";
  request += rp->vol;
  request += ":";
  request += rp->dist;
  request += " HTTP/1.0\r\n\r\n";

  //char req[] = "GET /cgi-bin/parnik_upd2?ts=1435608900&T=17.25:U:U:U:26:27:U&M=00:00&P=11.7:U&V=173.61:5.92 HTTP/1.0\r\n\r\n";
  len = request.length()+1;
  request.toCharArray(buf, len);

  if (gprs.join(apn, user, pass)) {
    //Serial.println("joined");
    if (gprs.connect (TCP, host, 80, 100)) {
      //Serial.println("Connected"); 
      gprs.send(buf, len);
      //Serial.println(buf);
      //Serial.print("req len=");
      //Serial.println(len);
      //Serial.println("Wait");
      len = gprs.recv(buf, sizeof(buf)); 
      buf[len+1] = 0;
      //Serial.println("Rcv ");    
      //Serial.println(buf);
      //Serial.print("len=");
      //Serial.println(len);
    } else {
      gprs.close();
      Serial.println("con f");
      return false;
    }  
    gprs.close();
    return true;
  } else {
    Serial.println("f join");
    return false;
  }
}


