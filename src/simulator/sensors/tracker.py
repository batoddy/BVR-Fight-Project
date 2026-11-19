"""Radar iz dosyası: her uçağın gördüğü uçakların son ölçümü (protokol v2.1 contacts).

Görülmeyen uçak son ölçümüyle kalır (tracked = false, age_s büyür); hiç görülmemiş uçak yoktur.
Sim tahmin yürütmez: kinematik son ölçümün aynısıdır.
"""


class Tracker:
    def __init__(self):
        self.tracks = {}                    # hedef anahtarı → {kinematics, time_s, tracked}

    def update(self, own, others, time_s):
        """Radar taramasını uygula; değişimleri olay listesi olarak döndür (radar_contact / radar_contact_lost).
        Sensörler kapalıysa (radar.realistic False) herkes her zaman görülür, olay üretilmez."""
        if not own.radar.realistic:
            self.tracks = {t.key: dict(kinematics=t.model.kinematics(), time_s=time_s, tracked=True) for t in others}
            return []
        events = []
        for target in others:
            seen = own.radar.sees(own, target)
            track = self.tracks.get(target.key)
            if seen:
                if track is None or not track["tracked"]:
                    events.append(dict(type="radar_contact", target_id=target.key))
                self.tracks[target.key] = dict(kinematics=target.model.kinematics(), time_s=time_s, tracked=True)
            elif track is not None and track["tracked"]:
                track["tracked"] = False
                events.append(dict(type="radar_contact_lost", target_id=target.key))
        return events

    def contacts(self, aircraft, time_s):
        """Protokol v2.1 contacts listesi."""
        return [dict(id=key, team=aircraft[key].team, tracked=track["tracked"],
                     source="radar" if aircraft[key].radar.realistic else "truth",   # bayrak bütün radarlarda aynı (world.sensors)
                     age_s=time_s - track["time_s"], kinematics=track["kinematics"])
                for key, track in self.tracks.items()]
