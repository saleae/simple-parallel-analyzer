#include "SimpleParallelAnalyzer.h"
#include "SimpleParallelAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <cassert>
#include <algorithm>

SimpleParallelAnalyzer::SimpleParallelAnalyzer()
    : Analyzer2(), mSettings( new SimpleParallelAnalyzerSettings() ), mSimulationInitilized( false )
{
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

SimpleParallelAnalyzer::~SimpleParallelAnalyzer()
{
    KillThread();
}

void SimpleParallelAnalyzer::SetupResults()
{
    mResults.reset( new SimpleParallelAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mClockChannel );
}

void SimpleParallelAnalyzer::WorkerThread()
{
    mSampleRateHz = GetSampleRate();

    AnalyzerResults::MarkerType clock_arrow;
    if( mSettings->mClockEdge == ParallelAnalyzerClockEdge::NegEdge )
        clock_arrow = AnalyzerResults::DownArrow;
    else
        clock_arrow = AnalyzerResults::UpArrow;


    mClock = GetAnalyzerChannelData( mSettings->mClockChannel );
    mChipSelect = GetAnalyzerChannelData( mSettings->mChipSelectChannel );
    
    mData.clear();
    mDataMasks.clear();
    mDataChannels.clear();

    U32 count = mSettings->mDataChannels.size();
    for( U32 i = 0; i < count; i++ )
    {
        if( mSettings->mDataChannels[ i ] != UNDEFINED_CHANNEL )
        {
            mData.push_back( GetAnalyzerChannelData( mSettings->mDataChannels[ i ] ) );
            mDataMasks.push_back( 1 << i );
            mDataChannels.push_back( mSettings->mDataChannels[ i ] );
        }
    }

    // Move the clock to the falling edge of CS.
    if ( mChipSelect->GetBitState() == BIT_HIGH) {
        mChipSelect->AdvanceToNextEdge(); // Falling
        mClock->AdvanceToAbsPosition(mChipSelect->GetSampleNumber());
    }

    U32 num_data_lines = mData.size();
    for( ;; )
    {
        S64 index = 0;
        bool end_with_cs = false;
        // Skip to the next inactive edge.
        if( (mSettings->mClockEdge == ParallelAnalyzerClockEdge::NegEdge && mClock->GetBitState() == BIT_LOW) ||
            (mSettings->mClockEdge == ParallelAnalyzerClockEdge::PosEdge && mClock->GetBitState() == BIT_HIGH) ) {
            mClock->AdvanceToNextEdge();
            end_with_cs = true;
        }

        while (!mChipSelect->WouldAdvancingToAbsPositionCauseTransition(mClock->GetSampleOfNextEdge())) {
            auto frame_start = mClock->GetSampleNumber();
            mClock->AdvanceToNextEdge(); // Active
            U64 sample = mClock->GetSampleNumber();

            mResults->AddMarker( sample, clock_arrow, mSettings->mClockChannel );

            U16 result = GetWordAtLocation( sample );

            FrameV2 frame_v2;
            frame_v2.AddInteger( "data", result );
            frame_v2.AddInteger( "index", index );
            index += 1;

            Frame frame;
            frame.mData1 = result;
            frame.mFlags = 0;
            frame.mStartingSampleInclusive = frame_start;

            // Move to next inactive edge or the end of cs.
            if (end_with_cs) {
                while (!mClock->DoMoreTransitionsExistInCurrentData() &&
                       !mChipSelect->DoMoreTransitionsExistInCurrentData()) {
                    // Wait for clock or chip select transitions.
                }
                // Only get the next edge of the signal we have a transition for. Once we do, make
                // sure the other signal doesn't transition earlier. Move mClock to the earlier of
                // the transitions to end the frame.
                if (mClock->DoMoreTransitionsExistInCurrentData()) {
                    if (mChipSelect->WouldAdvancingToAbsPositionCauseTransition(mClock->GetSampleOfNextEdge())) {
                        mClock->AdvanceToAbsPosition(mChipSelect->GetSampleOfNextEdge());
                    } else {
                        mClock->AdvanceToNextEdge();
                    }
                } else {
                    if (mClock->WouldAdvancingToAbsPositionCauseTransition(mChipSelect->GetSampleOfNextEdge())) {
                        mClock->AdvanceToNextEdge();
                    } else {
                        mClock->AdvanceToAbsPosition(mChipSelect->GetSampleOfNextEdge());
                    }
                }
            } else {
                mClock->AdvanceToNextEdge();
            }

            frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
            frame_start = mClock->GetSampleNumber();
            mResults->AddFrame( frame );
            mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
            mResults->CommitResults();
            ReportProgress( frame.mEndingSampleInclusive );
        }
        mChipSelect->AdvanceToNextEdge(); // Rising
        ReportProgress( mChipSelect->GetSampleNumber() );
        mChipSelect->AdvanceToNextEdge(); // Falling
        ReportProgress( mChipSelect->GetSampleNumber() );
        mClock->AdvanceToAbsPosition(mChipSelect->GetSampleNumber());
    }
}

bool SimpleParallelAnalyzer::NeedsRerun()
{
    return false;
}

U32 SimpleParallelAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate,
                                                    SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 SimpleParallelAnalyzer::GetMinimumSampleRateHz()
{
    return 1000000;
}

const char* SimpleParallelAnalyzer::GetAnalyzerName() const
{
    return "Simple Parallel w/CS";
}

uint16_t SimpleParallelAnalyzer::GetWordAtLocation( uint64_t sample_number )
{
    uint16_t result = 0;

    int num_data_lines = mData.size();

    for( int i = 0; i < num_data_lines; i++ )
    {
        mData[ i ]->AdvanceToAbsPosition( sample_number );
        if( mData[ i ]->GetBitState() == BIT_HIGH )
        {
            result |= mDataMasks[ i ];
        }
        mResults->AddMarker( sample_number, AnalyzerResults::Dot, mDataChannels[ i ] );
    }

    return result;
}

const char* GetAnalyzerName()
{
    return "Simple Parallel w/CS";
}

Analyzer* CreateAnalyzer()
{
    return new SimpleParallelAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}
