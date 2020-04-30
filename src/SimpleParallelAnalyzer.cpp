#include "SimpleParallelAnalyzer.h"
#include "SimpleParallelAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

SimpleParallelAnalyzer::SimpleParallelAnalyzer()
:	Analyzer2(),  
	mSettings( new SimpleParallelAnalyzerSettings() ),
	mSimulationInitilized( false )
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

	U32 count = mSettings->mDataChannels.size();
	for( U32 i=0; i<count; i++ )
	{
		if( mSettings->mDataChannels[i] != UNDEFINED_CHANNEL )
		{
			mData.push_back( GetAnalyzerChannelData( mSettings->mDataChannels[i] ) );
			mDataMasks.push_back( 1 << i );
			mDataChannels.push_back( mSettings->mDataChannels[i] );
		}
	}


	U32 num_data_lines = mData.size();

	if( mSettings->mClockEdge == AnalyzerEnums::NegEdge )
	{
		if( mClock->GetBitState() == BIT_LOW )
			mClock->AdvanceToNextEdge();
	}else
	{
		if( mClock->GetBitState() == BIT_HIGH )
			mClock->AdvanceToNextEdge();
	}


	mClock->AdvanceToNextEdge();  //this is the data-valid edge

	Frame last_frame;
	bool added_last_frame = false;
	for( ; ; )
	{
		
		//here we found a rising edge at the mark. Add images to mResults
		U64 sample = mClock->GetSampleNumber();
		mResults->AddMarker( sample, clock_arrow, mSettings->mClockChannel );

		U16 result = 0;

		for( U32 i=0; i<num_data_lines; i++ )
		{
			mData[i]->AdvanceToAbsPosition( sample );
			if( mData[i]->GetBitState() == BIT_HIGH )
			{
				result |= mDataMasks[i];
			}
			mResults->AddMarker( sample, AnalyzerResults::Dot, mDataChannels[i] );
		}	

		Frame frame;
		frame.mData1 = result;
		frame.mFlags = 0;
		frame.mStartingSampleInclusive = sample;

		if( added_last_frame || mClock->DoMoreTransitionsExistInCurrentData() ) //if this thread ever gets before the data worker thread then this condition may hit before the end of the dataset which will only cause a problem if the clock changed between packets. The last frame will use the old clock rate for ending sample
		{
			//wait until there is another rising edge so we know the ending sample
			mClock->AdvanceToNextEdge();

			if( mClock->DoMoreTransitionsExistInCurrentData() )
			{
				mClock->AdvanceToNextEdge();  //this is the data-valid edge
				added_last_frame = false;
			}
			else
				added_last_frame = true;

			frame.mEndingSampleInclusive = mClock->GetSampleNumber() - 1;
		}
		else
		{
			frame.mEndingSampleInclusive = frame.mStartingSampleInclusive + ( last_frame.mEndingSampleInclusive - last_frame.mStartingSampleInclusive);
			added_last_frame = true;
		}

		last_frame = frame;
		//finally, add the frame
		mResults->AddFrame( frame );
		mResults->CommitResults();
		ReportProgress( frame.mEndingSampleInclusive );
	}
}

bool SimpleParallelAnalyzer::NeedsRerun()
{
	return false;
}

U32 SimpleParallelAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
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
