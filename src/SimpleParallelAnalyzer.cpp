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

    if( mSettings->mClockEdge == ParallelAnalyzerClockEdge::NegEdge )
    {
        if( mClock->GetBitState() == BIT_LOW )
            mClock->AdvanceToNextEdge();
    }
    else if( mSettings->mClockEdge == ParallelAnalyzerClockEdge::NegEdge )
    {
        if( mClock->GetBitState() == BIT_HIGH )
            mClock->AdvanceToNextEdge();
    }
    else if( mSettings->mClockEdge == ParallelAnalyzerClockEdge::DualEdge )
    {
        // handling both edges is different enough to warrant a separate implementation.
        DecodeBothEdges();
        return;
    }

    mClock->AdvanceToNextEdge(); // this is the data-valid edge

    Frame last_frame;
    bool is_first_frame = true;
    for( ;; )
    {
        // We always start this loop on an active edge.

        U64 sample = mClock->GetSampleNumber();
        mResults->AddMarker( sample, clock_arrow, mSettings->mClockChannel );

        U16 result = GetWordAtLocation( sample );

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


void SimpleParallelAnalyzer::DecodeBothEdges()
{
    // helper to allow us to successfully report the last edge, even if there are no more edges in the capture.
    // Normally, in order to report a specific edge, the following edge must be found first.
    // If that following edge does not exist, and never will exist, instead we advance a intermediate amount, and return false.
    // If there is another edge, we advance to it and return true.
    // force_advance optionally ensures we advance to the next edge. If there are no more edges in the data, this will never return.
    auto advance_to_next_edge_or_fail = [&]( bool force_advance ) -> bool {
        if( mClock->DoMoreTransitionsExistInCurrentData() || force_advance )
        {
            mClock->AdvanceToNextEdge();
            return true;
        }
        int64_t estimated_frame_size = mLastFrameWidth > 0 ? std::max<int64_t>( static_cast<int64_t>( mLastFrameWidth * 0.1 ), 2 ) : 10;
        if( mClock->WouldAdvancingCauseTransition( estimated_frame_size ) )
        {
            // this condition will only be true if we're very lucky, and we're processing data while recording in real time.
            mClock->AdvanceToNextEdge();
            return true;
        }
        mClock->Advance( estimated_frame_size );
        return false;
    };

    // Frames start at the active edge, and extend to the following edge -1. (or they extend to an estimated width, if no more data is
    // found.)
    // we must advance past the active edge before committing the result from that edge.

    // has_pending_frame indicates that we have a word to store after the previous cycle.
    bool has_pending_frame = false;
    uint16_t previous_value = 0;
    uint64_t previous_sample = 0;

    for( ;; )
    {
        // advance to the next active edge.
        auto found_next_edge = advance_to_next_edge_or_fail( !has_pending_frame );
        auto location = mClock->GetSampleNumber();
        uint64_t progress_update = 0;
        if( has_pending_frame )
        {
            // store the previous frame.
            progress_update = AddFrame( previous_value, previous_sample, location - 1 );
            has_pending_frame = false;
        }
        if( found_next_edge )
        {
            has_pending_frame = true;
            previous_sample = location;
            previous_value = GetWordAtLocation( location );

            mResults->AddMarker( location, mClock->GetBitState() == BIT_LOW ? AnalyzerResults::DownArrow : AnalyzerResults::UpArrow,
                                 mSettings->mClockChannel );
        }

        if( progress_update > 0 )
        {
            ReportProgress( progress_update );
        }
    }
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

uint64_t SimpleParallelAnalyzer::AddFrame( uint16_t value, uint64_t starting_sample, uint64_t ending_sample )
{
    assert( starting_sample <= ending_sample );
    FrameV2 frame_v2;
    frame_v2.AddInteger( "data", value );

    Frame frame;
    frame.mData1 = value;
    frame.mFlags = 0;
    frame.mStartingSampleInclusive = starting_sample;
    frame.mEndingSampleInclusive = ending_sample;
    mResults->AddFrame( frame );
    mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
    mResults->CommitResults();
    mLastFrameWidth = std::max<uint64_t>( ending_sample - starting_sample, 1 );
    return ending_sample;
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
