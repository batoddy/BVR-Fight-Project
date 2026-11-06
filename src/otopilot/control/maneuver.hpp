#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

// =============================================================================
// ORTAK  —  bütün manevraların arayüzü ve uçurma yardımcısı
// =============================================================================

// Manevranın durumu.
enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
};

// Bütün manevraların ortak arayüzü. Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar;
// hangisinin çalışacağını otopilot seçer.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    /// Bu manevranın fazı (görüntüleyicide ve CSV'de görünen ad).
    virtual Phase phase() const = 0;

    /// Manevraya geçilince bir kez: girişe bağlı hedefleri kilitler.
    virtual void enter(const Aircraft& own) { (void)own; }

    /// Her step: own.command'dan (Python'un son uyguladığı) başlayıp yeni komutu döner.
    virtual FlightCommand update(const Aircraft& own) = 0;

    /// Kendi içinde biten manevralar (Out, Recommit) hedefe varınca Done döner;
    /// sürekli manevralar (Hold, Commit, Crank, Beam) hep Running döner, onları otopilot bitirir.
    /// Gaz şimdilik manevranın içinde: her manevra kendi THROTTLE seviyesini yazar (throttle::), Hold dokunmaz.
    virtual ManeuverStatus status(const Aircraft& own) const
    {
        (void)own;
        return ManeuverStatus::Running;
    }
};

// =============================================================================
// HOLD  —  şu anki yön ve irtifayla, aynı gazla devam
// =============================================================================

// NOT: her step ölçülen yön ve irtifayı hedef yapar; irtifa kayarsa düzeltilmez. Girişte kilitlemek ayrı bir karar.
class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }

    /// Yön ve irtifa = şu anki ölçüm, gaz aynı, g sınırı yok.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
// =============================================================================

class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }

    /// Hedef irtifayı kilitler: DESCEND_FT alçal, FLOOR_FT'nin altına inme.
    void enter(const Aircraft& own) override;

    /// Yön = bearing − 180, irtifa = kilitli hedef, tam art yakıcı, out g'si.
    FlightCommand update(const Aircraft& own) override;

    /// Düşman arkamda kaldıysa (|ATA| > DONE_ATA_DEG) Done.
    ManeuverStatus status(const Aircraft& own) const override;

private:
    static constexpr double DESCEND_FT = 3300.0;    // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;     // ft, out bunun altına indirmez
    static constexpr double THROTTLE = throttle::MAX_THRUST;
    static constexpr double DONE_ATA_DEG = 177.0;   // derece, |ATA| bunu geçince düşman arkada: bitti

    double target_altitude_ft_ = 0;                 // ft, enter()'da bir kez hesaplanır, out boyunca sabit
};

// =============================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
// =============================================================================

class Commit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Commit; }

    /// Yön = düşmanın bearing'i; irtifa aynı, gaz THROTTLE.
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
};


// Manevra yönü, havacılık terimiyle: uçağın döndüğü taraf (Crank ve Recommit).
enum class ManeuverSide
{
    Right,   // uçak sağa döner. Crank: düşman burnun SOLUNDA kalır (ATA −). Recommit: SAĞINDA kalır (ATA +).
    Left,    // uçak sola döner. Crank: düşman burnun SAĞINDA kalır (ATA +). Recommit: SOLUNDA kalır (ATA −).
};

// =============================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// =============================================================================

// Taraf girişte kısa dönüşle seçilir (düşman sağımdaysa Right) ve recommit boyunca sabit kalır.
class Recommit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Recommit; }

    /// Tarafı seçer: düşman sağımdaysa (ATA ≥ 0) Right, solumdaysa Left.
    void enter(const Aircraft& own) override;

    /// Yön = bearing ∓ hedef ATA (Right −, Left +) + gecikme payı; irtifa aynı, gaz THROTTLE.
    /// Hedef ATA atış açısına varana kadar recommit_fire_ata_deg − TURN_INSIDE_DEG, varınca recommit_fire_ata_deg.
    FlightCommand update(const Aircraft& own) override;

    /// Atış açısına geldiyse (|ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG) Done.
    ManeuverStatus status(const Aircraft& own) const override;

    /// Girişte seçilen taraf.
    ManeuverSide side() const { return side_; }

private:
    // Hedef tam atış açısı olsaydı yön hatası küçüldükçe dönüş yumuşar, 40°'den 30°'ye iniş uzardı.
    // Hedef atış açısının TURN_INSIDE_DEG içeride tutulur (30 − 10 = 20°): dönüş 30°'ye kadar sert kalır;
    // 30°'ye varınca (status Done) hedef atış açısına alınır ve uçak orada durur.
    static constexpr double TURN_INSIDE_DEG = 10.0;      // derece, varana kadar hedefin atış açısından içeride tutulduğu açı
    static constexpr double DONE_TOLERANCE_DEG = 1.0;    // derece, |ATA| atış açısını bu kadar aşsa da varılmış sayılır
    static constexpr double THROTTLE = throttle::MAX_THRUST;

    ManeuverSide side_ = ManeuverSide::Left;   // enter()'da seçilir
};

// =============================================================================
// CRANK —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// =============================================================================

// Menzil kapanmasını yavaşlatır.
// Taraf girişte seçilir ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }

    /// Tarafı seçer: TA ≥ 0 (düşmanın burnunun sağındayım) ise Left, değilse Right.
    /// Seçimi düşmanın nereye gittiği belirler; düşmanın benim burnuma göre konumu değil.
    void enter(const Aircraft& own) override;

    /// Yön = bearing ± (gimbal − OFFSET_DEG) (Right +, Left −) + gecikme payı; irtifa aynı, gaz THROTTLE.
    FlightCommand update(const Aircraft& own) override;

    /// Düşman radarın görüş konisinde mi (|ATA| < gimbal sınırı).
    bool in_radar_sight(const Aircraft& own) const;

    /// Girişte seçilen taraf.
    ManeuverSide side() const { return side_; }

private:
    static constexpr double OFFSET_DEG = 5.0;   // derece, gimbal sınırından içeri pay
    static constexpr double THROTTLE = throttle::DEFAULT;

    ManeuverSide side_ = ManeuverSide::Left;   // enter()'da seçilir
};

// =============================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
// =============================================================================

// Taraf burnuma göre seçilir: düşman sağımdaysa sola, solumdaysa sağa; dönüş hep 90°'den kısa.
// Füzeye göre olan notch ayrı bir manevra olacak.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }

    /// Yön = bearing − 90 (düşman sağımda) ya da bearing + 90 (solumda); irtifa aynı, gaz THROTTLE.
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
};

// =============================================================================
// NOTCH  —  füzeye dik uç (İSKELET)
// =============================================================================

// Beam'in füzeye göre olanı: düşman uçağın değil, bana gelen füzenin (own.picture.threat) 3/9 hattı.
class Notch : public Maneuver
{
public:
    Phase phase() const override { return Phase::Notch; }

    /// Örnek: yön = füzenin bearing'i ∓ 90; füze yoksa komut değişmez.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// DRAG  —  füzeye sırt dön (İSKELET)
// =============================================================================

// Out'un füzeye göre olanı: füzeyi tam arkaya al, kapanma hızını düşür.
class Drag : public Maneuver
{
public:
    Phase phase() const override { return Phase::Drag; }

    /// Örnek: yön = füzenin bearing'i − 180; füze yoksa komut değişmez.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç (İSKELET)
// =============================================================================

// Açı ve taraf taktikten gelir: maneuvers.offset.set(40.0, ManeuverSide::Left) sonra fly.
class Offset : public Maneuver
{
public:
    Phase phase() const override { return Phase::Offset; }

    /// Açıyı ve tarafı ayarlar; fly'dan önce çağrılır.
    void set(double offset_deg, ManeuverSide side);

    /// Örnek: yön = bearing ± offset_deg_ (Right +, Left −).
    FlightCommand update(const Aircraft& own) override;

private:
    double offset_deg_ = 0;                    // derece, burnun düşman yönünden sapması
    ManeuverSide side_ = ManeuverSide::Left;
};

// =============================================================================
// FLANK  —  düşmanın burnuna göre yan açıda kal (İSKELET)
// =============================================================================

// Aralık condition::Aspect::Flank'ten (30-70°). Açık soru: ta_deg mi (düşmanın burnu) ata_deg mi (benim burnum).
class Flank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Flank; }

    /// Örnek: |ta| hedef açının altındaysa düşmanın burnundan uzağa doğru dön.
    FlightCommand update(const Aircraft& own) override;

    /// Örnek: |ta| hedef aralığa girince Done.
    ManeuverStatus status(const Aircraft& own) const override;

private:
    static constexpr double TARGET_TA_DEG = 50.0;   // derece, Flank aralığının ortası
};

// =============================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası (İSKELET)
// =============================================================================

// Bearing değişmiyorsa iki uçak aynı noktaya varır; en kısa yoldan kesişme.
class Collision : public Maneuver
{
public:
    Phase phase() const override { return Phase::Collision; }

    /// Örnek: yön = bearing + KP × los_rate_deg_s.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// FLY HEADING  —  verilen sabit yöne uç (İSKELET)
// =============================================================================

// Flow cold, Reset ve Departure için: yön taktikten gelir.
class FlyHeading : public Maneuver
{
public:
    Phase phase() const override { return Phase::FlyHeading; }

    /// Yönü ayarlar; fly'dan önce çağrılır.
    void set(double heading_deg);

    /// Örnek: yön = heading_deg_; irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;

private:
    double heading_deg_ = 0;   // derece ±180
};

// =============================================================================
// SNAKE  —  periyodik sağ-sol (İSKELET)
// =============================================================================

// Zamana bağlı: saat World::sim_time_s'te var ama otopilota iletilmiyor; iletilince kullanılır.
class Snake : public Maneuver
{
public:
    Phase phase() const override { return Phase::Snake; }

    /// Örnek: girişteki yönü ve başlangıç zamanını kilitler.
    void enter(const Aircraft& own) override;

    /// Örnek: yön = base_heading_deg_ ± AMPLITUDE_DEG, her HALF_PERIOD_S'de taraf değişir.
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double AMPLITUDE_DEG = 30.0;   // derece, ana yönden sapma
    static constexpr double HALF_PERIOD_S = 10.0;   // s, bir tarafta kalma süresi

    double base_heading_deg_ = 0;   // derece, enter()'da kilitlenir
};

// =============================================================================
// PURSUIT  —  pure, lag, lead takip (İSKELET)
// =============================================================================

enum class PursuitMode
{
    Pure,   // burun düşmanda; bugünkü Commit ile aynı
    Lag,    // burun düşmanın gerisinde: menzil yavaş kapanır, aşma riski az
    Lead,   // burun düşmanın önünde: menzil hızlı kapanır
};

// Tek sınıf, üç mod: maneuvers.pure_pursuit, lag_pursuit, lead_pursuit.
class Pursuit : public Maneuver
{
public:
    explicit Pursuit(PursuitMode mode) : mode_(mode) {}

    Phase phase() const override;

    /// Örnek: Pure yön = bearing; Lag yön = bearing ∓ LAG_DEG; Lead yön = bearing ± öncü açı.
    FlightCommand update(const Aircraft& own) override;

private:
    static constexpr double LAG_DEG = 15.0;   // derece, Lag'de burnun düşmanın gerisinde kalacağı açı

    PursuitMode mode_;
};

// =============================================================================
// MANEUVERS  —  bir otopilotun manevra nesneleri
// =============================================================================

// Her otopilot kendi manevralarını ve son uçurduğu manevrayı tutar.
// Yeni manevra: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
class Maneuvers
{
public:
    /// Manevra değiştiyse enter()'ı bir kez çağırır; ardından fazı ve uçuş komutunu yazar.
    void fly(Aircraft& own, Maneuver& maneuver);

    Hold hold;     // rotayı koru
    Out out;       // kaç
    Commit commit; // düşmana doğru düz uç
    Recommit recommit;   // taraf girişte seçilir
    Crank crank;   // taraf girişte seçilir
    Beam beam;     // düşmana dik uç

    // İskelet: gövdeleri henüz komutu değiştirmez.
    Notch notch;                                   // füzeye dik uç
    Drag drag;                                     // füzeye sırt dön
    Offset offset;                                 // düşman yönünden açı kadar yana
    Flank flank;                                   // düşmanın burnuna göre yan açı
    Collision collision;                           // çarpışma rotası
    FlyHeading fly_heading;                        // sabit yön
    Snake snake;                                   // periyodik sağ-sol
    Pursuit pure_pursuit{PursuitMode::Pure};       // burun düşmanda
    Pursuit lag_pursuit{PursuitMode::Lag};         // burun düşmanın gerisinde
    Pursuit lead_pursuit{PursuitMode::Lead};       // burun düşmanın önünde

private:
    Maneuver* active_ = nullptr;   // son uçurulan manevra; ilk fly() da giriş sayılır
};
