
#include <iostream>
#include <fstream>
#include <ctime>
#include "svector.h"
#include "color.h"
#include "object.h"
#include "sphere.h"
#include "viewport.h"
#include "light.h"
#include "defs.h"
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#ifdef SRAY_MPI
#	include <mpi.h>
#elif defined (SRAY_OPENMP)
#	include <omp.h>
#endif

using namespace std;

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		cout << "usage: ./simpleray [config_file_name] [output_file_name] [(optional)config_output_file_name]" << endl;
		return 1;
	}

#if (defined(SRAY_MPI) && defined(SRAY_OPENMP))
	cout << "Hybrid model not supported." << endl;
	return 1;
#endif

	TViewport *aViewport = new TViewport;
	double t, t1, t2;
	int size = -1, rank = -1;

#ifdef SRAY_MPI   
   MPI_Status status;        
   MPI_Request request;       

   MPI_Init( &argc, &argv );
   MPI_Comm_rank( MPI_COMM_WORLD, &rank );
   MPI_Comm_size( MPI_COMM_WORLD, &size );
#endif


aViewport->ConfigureFromFile(argv[1]);

#ifdef SRAY_MPI   
	t1 = MPI_Wtime();
#else
	t1 = timef_() / 1000.0;
#endif

#ifdef SRAY_OPENMP
#	pragma omp parallel shared (aViewport, t) private (rank, size) default(none)
	{
		rank = omp_get_thread_num();
		size = omp_get_num_threads();
#endif
		aViewport->Render(size, rank);

#ifdef SRAY_OPENMP
	}
#endif

#ifdef SRAY_MPI
	t1 = MPI_Wtime() - t1;

	double t1_recv;
	MPI_Reduce(&t1, &t1_recv, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
if(!rank)
	cout << "Rendering time required " << t1_recv << " seconds." << endl;
#else
	t2 = timef_() / 1000.0;
	cout << "Rendering time required " << t2-t1 << " seconds." << endl;
#endif

#ifdef SRAY_MPI
	t2 = MPI_Wtime();
	if(!aViewport->DataCollection(rank, size))
	{
		delete aViewport;
		MPI_Finalize();
		return 1;
	}
	
	t2 = MPI_Wtime() - t2;
	double t2_recv;
MPI_Reduce(&t2, &t2_recv, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
if(!rank)
{
	cout << "Data communication time required " << t2_recv << " seconds." << endl;
	cout << "Total time required " << t1_recv + t2_recv << " seconds." << endl;
}


	if(!rank)   
#endif
	{
		aViewport->SaveToTGAFile(argv[2], size);
	}

#ifdef LOAD_MEASUREMENT
#	ifdef SRAY_MPI
		if(!aViewport->LoadDataCollection(rank, size))
		{
			delete aViewport;
			MPI_Finalize();
			return 1;
		}

		if(!rank)  
#  endif
		{
			aViewport->SaveLoadToFile("loadfile.txt", size, false);
#	if defined(SRAY_MPI) || defined(SRAY_OPENMP)
			aViewport->SaveLoadToFile("load_breakdown.txt", size, true);
#	endif	
		}
#endif

	if (argc == 4)
	{
#ifdef SRAY_MPI
		if (!rank)	
#endif
		{
			aViewport->SaveConfigToFile(argv[3]);
		}
	}

	delete aViewport;

#ifdef SRAY_MPI   
	MPI_Finalize();
#endif

	return 0;
}

