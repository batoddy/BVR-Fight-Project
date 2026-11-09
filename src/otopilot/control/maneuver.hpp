#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

// =============================================================================
// ORTAK  —  bütün manevraların arayüzü ve uçurma yardımcısı
// =============================================================================

enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
    Satisfied, // sürüyor ve şu an istenen koşulu sağlıyor; koşul bozulunca Running'e döner
};

// Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar; hangisinin çalışacağını otopilot seçer.
// Gaz ve g sınırı manevranın içindedir: her manevra kendi THROTTLE (throttle::) ve G_LIMIT değerini yazar.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    virtual Phase phase() const = 0;                                   // görüntüleyicide ve CSV'de görünen ad
    virtual void enter(const Aircraft& own) { (void)own; }             // manevraya geçilince bir kez: girişe bağlı hedefleri kilitler
    virtual FlightCommand update(const Aircraft& own) = 0;             // her step: own.command'dan başlayıp yeni komutu döner
    virtual ManeuverStatus status(const Aircraft& own) const           // varsayılan Running; açı tutanlar Satisfied, bitenler Done bildirebilir
    {
        (void)own;
        return ManeuverStatus::Running;
    }

    bool is_satisfied(const Aircraft& own) const                      // yalnız mevcut koşulun sağlanması; Done ayrı bir durumdur
    {
        return status(own) == ManeuverStatus::Satisfied;
    }
};

// Manevra yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class ManeuverSide
{
    Right,   // uçak sağa döner. Crank: düşman burnun SOLUNDA kalır (ATA −). Recommit: SAĞINDA kalır (ATA +).
    Left,    // uçak sola döner. Crank: düşman burnun SAĞINDA kalır (ATA +). Recommit: SOLUNDA kalır (ATA −).
};

// =============================================================================
// HOLD  —  girişteki yön ve irtifayı koru
// =============================================================================

class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }
    void enter(const Aircraft& own) override;             // ölçülen yön ve irtifayı bir kez kilitler
    FlightCommand update(const Aircraft& own) override;   // kilitlenen yön ve irtifaya geri döner
    ManeuverStatus status(const Aircraft& own) const override; // yön ve irtifa tolerans içindeyse Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
    static constexpr double ALTITUDE_TOLERANCE_FT = 100.0;

    double target_heading_deg_ = 0;                       // derece, girişteki yön | enter
    double target_altitude_ft_ = 0;                       // ft, girişteki irtifa | enter
    bool has_target_ = false;
};

// =============================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca Done, uçulmaya devam ederse kaçış sürer
// =============================================================================

class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }
    void enter(const Aircraft& own) override;                     // hedef irtifayı kilitler: DESCEND_FT alçal, FLOOR_FT altına inme
    FlightCommand update(const Aircraft& own) override;           // yön = bearing − 180 + gecikme payı, irtifa = kilitli hedef, out g'si
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| > DONE_ATA_DEG: düşman arkada, Done

private:
    static constexpr double DESCEND_FT = 3300.0;                  // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;                   // ft, out bunun altına indirmez
    static constexpr double THROTTLE = throttle::MAX_THRUST;      // Done'dan sonra da: COLD ve ABORT aynı out'u sürdürür
    static constexpr double DONE_ATA_DEG = 177.0;                 // derece, |ATA| bunu geçince düşman arkada

    double target_altitude_ft_ = 0;                               // ft, enter()'da bir kez hesaplanır
};

// =============================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
// =============================================================================

class Commit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Commit; }
    FlightCommand update(const Aircraft& own) override;   // yön = düşmanın bearing'i; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // burun hedef yönüne tolerans içinde bakıyorsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                // g, yumuşak dönüş: hız ve irtifa korunur
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};

// =============================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// =============================================================================

// Hedef tam atış açısı (30°) olsaydı yön hatası küçüldükçe dönüş yumuşar, 40°'den 30°'ye iniş uzardı.
// Bu yüzden ilk varışa kadar hedef TURN_INSIDE_DEG içeride (20°) tutulur, varınca 30°'ye alınır ve orada kalınır.
class Recommit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Recommit; }
    void enter(const Aircraft& own) override;                     // taraf: düşman sağımdaysa Right, solumdaysa Left; varış bayrağı false
    FlightCommand update(const Aircraft& own) override;           // yön = bearing ∓ hedef ATA + gecikme payı; irtifa aynı, g out_g
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG: Done
    ManeuverSide side() const { return side_; }                   // girişte seçilen taraf

private:
    static constexpr double TURN_INSIDE_DEG = 10.0;               // derece, varana kadar hedefin atış açısından içeride tutulduğu açı
    static constexpr double DONE_TOLERANCE_DEG = 1.0;             // derece, |ATA| atış açısını bu kadar aşsa da varılmış sayılır
    static constexpr double THROTTLE = throttle::MAX_THRUST;

    ManeuverSide side_ = ManeuverSide::Left;                      // enter()'da seçilir
    bool has_reached_ = false;                                    // atış açısına bir kez varıldı mı
};

// =============================================================================
// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// =============================================================================

// Taraf girişte düşmanın gidişine göre seçilir (benim burnuma göre değil) ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }
    void enter(const Aircraft& own) override;                 // taraf: TA ≥ 0 (düşmanın burnunun sağındayım) ise Left, değilse Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing ± (gimbal − OFFSET_DEG) + gecikme payı; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta hedef ATA tolerans içindeyse ve radar görüyorsa Satisfied
    bool in_radar_sight(const Aircraft& own) const;           // düşman radarın görüş konisinde mi (|ATA| < gimbal)
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double OFFSET_DEG = 3.0;                 // derece, gimbal sınırından içeri pay
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;        // derece, hedef ATA'dan izin verilen sapma
    static constexpr double THROTTLE = throttle::CRANK;
    static constexpr double G_LIMIT = 3.0;                    // g, yumuşak dönüş (flight_control: 3 g crank 50° ≈ 14.5 s)

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};

// =============================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
// =============================================================================

// Taraf girişte burnuma göre seçilir ve sabit kalır; dönüş hep 90°'den kısa. Füzeye göre olanı Notch.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }
    void enter(const Aircraft& own) override;                 // taraf: düşman sağımdaysa (ATA > 0) Left, solumdaysa Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing − 90 (Left) / + 90 (Right) + gecikme payı; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta ATA ±90°'ye yakınsa Satisfied
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                    // g, yumuşak dönüş
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};

// =============================================================================
// NOTCH  —  füzeye dik uç
// =============================================================================

// Beam'in füzeye göre olanı: bana gelen füzenin (own.picture.threat) 3/9 hattı; füze yoksa düşman uçağın.
class Notch : public Maneuver
{
public:
    Phase phase() const override { return Phase::Notch; }
    FlightCommand update(const Aircraft& own) override;   // yön = tehdidin bearing'i ∓ 90, kısa dönüş; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;   // tehdit ATA ±90°'ye yakınsa Satisfied, değilse Running

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};

// =============================================================================
// DRAG  —  füzeye sırt dön
// =============================================================================

// Out'un füzeye göre olanı: füzeyi tam arkaya al, kapanma hızını düşür; füze yoksa düşman uçağı arkaya alır.
class Drag : public Maneuver
{
public:
    Phase phase() const override { return Phase::Drag; }
    FlightCommand update(const Aircraft& own) override;   // yön = tehdidin bearing'i − 180; g out_g; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;   // tehdit ATA ±180°'ye yakınsa Satisfied, değilse Running

private:
    static constexpr double THROTTLE = throttle::MAX_THRUST;     // P-1290: drag askeri güçle
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};

// =============================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç
// =============================================================================

// Açı ve taraf taktikten gelir: maneuvers.offset.set(40.0, ManeuverSide::Left) sonra fly.
class Offset : public Maneuver
{
public:
    Phase phase() const override { return Phase::Offset; }
    void set(double offset_deg, ManeuverSide side);       // fly'dan önce çağrılır; açı sonlu ve 0..180 derece olmalıdır
    FlightCommand update(const Aircraft& own) override;   // yön = bearing ± offset_deg_ + LOS gecikme payı (Right +, Left −)
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta hedef ATA'ya yakınsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double offset_deg_ = 0;                               // derece, burnun düşman yönünden sapması
    ManeuverSide side_ = ManeuverSide::Left;
};

// =============================================================================
// FLANK  —  düşmanın burnuna göre verilen yan açıyı koru
// =============================================================================

// maneuvers.flank.set(40.0): düşmanın 40° sağı; set(-40.0): 40° solu. Açıya varınca da uçmaya devam eder.
class Flank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Flank; }
    void set(double target_ta_deg, double tolerance_deg = 3.0);    // hedef TA ±180'e sarılır; tolerans 0..180 derece olmalıdır
    void enter(const Aircraft& own) override;                     // girişteki yatay menzil kilitlenir
    FlightCommand update(const Aircraft& own) override;           // hareketli hedefin verilen yan konumuna, kilitli menzilde yönelir; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;    // TA hedefin toleransı içindeyse Satisfied, dışında Running; Done olmaz

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                        // g, konum düzeltme dönüşü
    static constexpr double RESPONSE_S = 20.0;                    // s, yan konum hatasını giderme süresi; simülasyon ayarı

    double target_ta_deg_ = 50.0;                                // derece, düşmanın burnuna göre istenen işaretli açı | set
    double tolerance_deg_ = 3.0;                                 // derece, hedef açı çevresindeki kabul aralığı | set
    double range_nm_ = 0;                                        // NM, düşmandan tutulacak yatay menzil | enter
};

// =============================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası
// =============================================================================

// Menzil kapanırken sabit bearing kesişme rotasını gösterir; LOS hızına göre öncü açı verir.
class Collision : public Maneuver
{
public:
    Phase phase() const override { return Phase::Collision; }
    FlightCommand update(const Aircraft& own) override;   // kapanırken mevcut yöne LOS düzeltmesi; uzaklaşıyorsa önce hedefe dön
    ManeuverStatus status(const Aircraft& own) const override; // menzil kapanıyor ve yatay LOS hızı tolerans içindeyse Satisfied

private:
    static constexpr double KP_S = 10.0;                  // s, LOS hızını öncü açıya çeviren kazanç; simülasyonda ayarlanacak
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double LOS_RATE_TOLERANCE_DEG_S = 0.1; // derece/s, sabit yatay bearing toleransı
};

// =============================================================================
// FLY HEADING  —  verilen sabit yöne uç
// =============================================================================

// Flow cold, Reset ve Departure için: yön taktikten gelir.
class FlyHeading : public Maneuver
{
public:
    Phase phase() const override { return Phase::FlyHeading; }
    void set(double heading_deg);                         // fly'dan önce çağrılır; yönü ±180'e sarıp saklar
    FlightCommand update(const Aircraft& own) override;   // yön = heading_deg_; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // ölçülen yön verilen açıya yakınsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double heading_deg_ = 0;                              // derece ±180
};

// =============================================================================
// SNAKE  —  periyodik sağ-sol (İSKELET)
// =============================================================================

// Zamana bağlı: saat World::sim_time_s'te var ama otopilota iletilmiyor; iletilince kullanılır.
class Snake : public Maneuver
{
public:
    Phase phase() const override { return Phase::Snake; }
    void enter(const Aircraft& own) override;             // örnek: girişteki yönü ve başlangıç zamanını kilitler
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = ana yön ± AMPLITUDE_DEG, her HALF_PERIOD_S'de taraf değişir

private:
    static constexpr double AMPLITUDE_DEG = 30.0;         // derece, ana yönden sapma
    static constexpr double HALF_PERIOD_S = 10.0;         // s, bir tarafta kalma süresi
    static constexpr double THROTTLE = throttle::DEFAULT;

    double base_heading_deg_ = 0;                         // derece, enter()'da kilitlenir
};

// =============================================================================
// PURSUIT  —  pure, lag, lead takip
// =============================================================================

enum class PursuitMode
{
    Pure,   // burun düşmanda; bugünkü Commit ile aynı
    Lag,    // burun düşmanın gerisinde: menzil yavaş kapanır, aşma riski az
    Lead,   // burun düşmanın önünde: menzil hızlı kapanır
};

// Tek sınıf, üç mod: maneuvers.pursuit.set(PursuitMode::Lag) sonra fly.
class Pursuit : public Maneuver
{
public:
    void set(PursuitMode mode, double throttle = throttle::DEFAULT);   // fly'dan önce çağrılır

    Phase phase() const override;                         // moda göre PurePursuit, LagPursuit, LeadPursuit
    FlightCommand update(const Aircraft& own) override;   // Pure bearing; Lag LOS yönünün gerisine LAG_DEG; Lead önüne LEAD_DEG; LOS dönmüyorsa pure
    ManeuverStatus status(const Aircraft& own) const override; // ölçülen yön ilgili modun hedef yönüne yakınsa Satisfied

private:
    static constexpr double LAG_DEG = 15.0;               // derece, Lag'de burnun düşmanın gerisinde kalacağı açı
    static constexpr double LEAD_DEG = 20.0;              // derece, Lead'de LOS hareketi yönündeki sabit öncü açı
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double target_heading_deg(const Aircraft& own) const;  // derece, ilgili modun güncel hedef yönü

    PursuitMode mode_ = PursuitMode::Pure;
    double throttle_ = throttle::DEFAULT;
};

// =============================================================================
// MANEUVERS  —  bir otopilotun manevra nesneleri
// =============================================================================

// Yeni manevra: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
class Maneuvers
{
public:
    void fly(Aircraft& own, Maneuver& maneuver);   // manevra değiştiyse enter() bir kez; sonra fazı ve komutu yazar

    Hold hold;                                     // rotayı koru
    Out out;                                       // kaç
    Commit commit;                                 // düşmana doğru düz uç
    Recommit recommit;                             // atış açısına dön
    Crank crank;                                   // radar sınırında yanal kaçış
    Beam beam;                                     // düşmana dik uç

    Notch notch;                                   // füzeye dik uç
    Drag drag;                                     // füzeye sırt dön
    Pursuit pursuit;                               // takip: Pure, Lag ya da Lead (set)
    Collision collision;                           // çarpışma rotası
    FlyHeading fly_heading;                        // sabit yön
    Flank flank;                                   // düşmanın verilen yan açısını koru
    Offset offset;                                 // düşman yönünden açı kadar yana

    // İskelet: gövdeleri henüz yalnız gazı yazar.
    Snake snake;                                   // periyodik sağ-sol

private:
    Maneuver* active_ = nullptr;                   // son uçurulan manevra; ilk fly() da giriş sayılır
};
