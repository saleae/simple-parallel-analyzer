#include "SimpleParallelAnalyzer.h"
#include "SimpleParallelAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

SimpleParallelAnalyzer::SimpleParallelAnalyzer()
    : Analyzer2(), mSettings( new SimpleParallelAnalyzerSettings() ), mSimulationInitilized( false )
{
    SetAnalyzerSettings( mSettings.get() );
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
    if( mSettings->mClockEdge == AnalyzerEnums::NegEdge )
        clock_arrow = AnalyzerResults::DownArrow;
    else
        clock_arrow = AnalyzerResults::UpArrow;


    mClock = GetAnalyzerChannelData( mSettings->mClockChannel );
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


    U32 num_data_lines = mData.size();

    if( mSettings->mClockEdge == AnalyzerEnums::NegEdge )
    {
        if( mClock->GetBitState() == BIT_LOW )
            mClock->AdvanceToNextEdge();
    }
    else
    {
        if( mClock->GetBitState() == BIT_HIGH )
            mClock->AdvanceToNextEdge();
    }

    mClock->AdvanceToNextEdge(); // this is the data-valid edge

    Frame last_frame;
    bool is_first_frame = true;
    for( ;; )
    {
        // We always start this loop on an active edge.

        U64 sample = mClock->GetSampleNumber();
        mResults->AddMarker( sample, clock_arrow, mSettings->mClockChannel );

        U16 result = 0;

        for( U32 i = 0; i < num_data_lines; i++ )
        {
            mData[ i ]->AdvanceToAbsPosition( sample );
            if( mData[ i ]->GetBitState() == BIT_HIGH )
            {
                result |= mDataMasks[ i ];
            }
            mResults->AddMarker( sample, AnalyzerResults::Dot, mDataChannels[ i ] );
        }


        FrameV2 frame_v2;
        frame_v2.AddInteger( "data", result );

        Frame frame;
        frame.mData1 = result;
        frame.mFlags = 0;
        frame.mStartingSampleInclusive = sample;

        // The code in these if/else blocks could be replaced with 2 `AdvanceToNextEdge` calls, but if no more transitions are encountered,
        // the current state will never be output as a frame. These blocks will detect that case in the available data, and output a frame
        // immediately, and then another one once the next active edge sample is known.
        if( !mClock->DoMoreTransitionsExistInCurrentData() )
        {
            // There are no transitions available in the current data. Estimate what the frame size might be as 10 if we haven't seen any
            // frames yet, otherwise use 10% of the last frame size.
            U64 estimated_frame_size = 10;
            if( !is_first_frame )
            {
                U64 last_frame_sample_count = last_frame.mEndingSampleInclusive - last_frame.mStartingSampleInclusive + 1;
                estimated_frame_size = last_frame_sample_count * 0.1;
                if( estimated_frame_size < 3 )
                {
                    estimated_frame_size = 3;
                }
            }

            // Make sure we haven't gone past the end of the _real_ frame. WouldAdvancingCauseTransition can block, in which case more
            // transitions might show up.
            if( mClock->WouldAdvancingCauseTransition( estimated_frame_size ) )
            {
                // Move to inactive edge
                mClock->AdvanceToNextEdge();
                if( mClock->DoMoreTransitionsExistInCurrentData() )
                {
                    // Move to active edge
                    mClock->AdvanceToNextEdge();

                    frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
                    mResults->AddFrame( frame );
                    mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
                    mResults->CommitResults();
                }
                else
                {
                    frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
                    mResults->AddFrame( frame );
                    mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
                    mResults->CommitResults();

                    // Move to active edge
                    mClock->AdvanceToNextEdge();
                }
            }
            else
            {
                frame.mEndingSampleInclusive = sample + estimated_frame_size - 1;
                if( frame.mEndingSampleInclusive <= frame.mStartingSampleInclusive )
                {
                    frame.mEndingSampleInclusive = frame.mStartingSampleInclusive + 1;
                }
                mResults->AddFrame( frame );
                mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
                mResults->CommitResults();

                // Move to inactive edge, and then the active edge
                mClock->AdvanceToNextEdge();
                mClock->AdvanceToNextEdge();
            }
        }
        else
        {
            // Move to inactive edge
            mClock->AdvanceToNextEdge();

            if( mClock->DoMoreTransitionsExistInCurrentData() )
            {
                // Move to active edge
                mClock->AdvanceToNextEdge();

                frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
                mResults->AddFrame( frame );
                mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
                mResults->CommitResults();
            }
            else
            {
                frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
                mResults->AddFrame( frame );
                mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
                mResults->CommitResults();

                // Move to active edge
                mClock->AdvanceToNextEdge();
            }
        }

        // Note: mClock should always be at an active edge at this point, and `frame` should have been added.

        is_first_frame = false;
        last_frame = frame;

        ReportProgress( frame.mEndingSampleInclusive );
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
    return "Simple Parallel";
}

const char* GetAnalyzerName()
{
    return "Simple Parallel";
}

Analyzer* CreateAnalyzer()
{
    return new SimpleParallelAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}
