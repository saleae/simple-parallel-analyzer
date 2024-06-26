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


    std::vector<Channel> mDataChannels;
    Channel mClockChannel;
    Channel mChipSelectChannel;

    ParallelAnalyzerClockEdge mClockEdge;

  protected:
    std::vector<AnalyzerSettingInterfaceChannel*> mDataChannelsInterface;

    std::unique_ptr<AnalyzerSettingInterfaceChannel> mClockChannelInterface;
    std::unique_ptr<AnalyzerSettingInterfaceChannel> mChipSelectChannelInterface;
    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mClockEdgeInterface;
};

#endif // SIMPLEPARALLEL_ANALYZER_SETTINGS
