#ifndef SIMPLEPARALLEL_ANALYZER_SETTINGS
#define SIMPLEPARALLEL_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

// originally from AnalyzerEnums::EdgeDirection { PosEdge, NegEdge };
enum class ParallelAnalyzerClockEdge
{
    PosEdge = AnalyzerEnums::PosEdge,
    NegEdge = AnalyzerEnums::NegEdge,
    DualEdge
};

class SimpleParallelAnalyzerSettings : public AnalyzerSettings
{
  public:
    SimpleParallelAnalyzerSettings();
    virtual ~SimpleParallelAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    void UpdateInterfacesFromSettings();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();
    U32 dataBits() const
    {
        return mDataBits;
    }


    std::vector<Channel> mDataChannels;
    Channel mClockChannel;

    ParallelAnalyzerClockEdge mClockEdge;

  protected:
    U32 MostSiginificantBitPosition() const;

    std::vector<AnalyzerSettingInterfaceChannel*> mDataChannelsInterface;

    std::unique_ptr<AnalyzerSettingInterfaceChannel> mClockChannelInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mClockEdgeInterface;

    U32 mDataBits; // valid channel number of mDataChannels
};

#endif // SIMPLEPARALLEL_ANALYZER_SETTINGS
